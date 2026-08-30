"""Class-keyed multi-signifier role treatments for outline rendering.

Color must not be the only channel when multiple roles are configured.
Secondary signifiers are stroke pattern and/or a geometric marker shape.
"""

from __future__ import annotations

import json
import math
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType
from typing import Any, Literal, cast

import numpy as np

from cs2_vision_access.domain.outline import (
    DRAW_COORDINATE_SCALE,
    DRAW_COORDINATE_SHIFT,
    OutlineStyle,
    parse_hex_bgr,
)

ROLE_MARKERS = frozenset({"none", "triangle", "square", "chevron"})
RoleMarker = Literal["none", "triangle", "square", "chevron"]
ROLE_CATALOG_SCHEMA_VERSION = 1

_STYLE_JSON_KEYS = frozenset(
    {
        "inner_color",
        "outer_color",
        "inner_width",
        "outer_width",
        "fill_opacity",
        "scale_with_frame",
        "stroke_pattern",
        "dash_period_px",
        "fill_mode",
        "halo_blur",
        "adapt_width_to_area",
        "outline_kernel",
    }
)


@dataclass(frozen=True)
class RoleTreatment:
    """One role's outline style plus optional non-color marker."""

    style: OutlineStyle
    marker: RoleMarker = "none"
    marker_scale: float = 1.0

    def __post_init__(self) -> None:
        if not isinstance(self.style, OutlineStyle):
            raise ValueError("style must be an OutlineStyle")
        if self.marker not in ROLE_MARKERS:
            choices = ", ".join(sorted(ROLE_MARKERS))
            raise ValueError(f"marker must be one of: {choices}; got {self.marker!r}")
        if (
            isinstance(self.marker_scale, bool)
            or not isinstance(self.marker_scale, (int, float))
            or not math.isfinite(float(self.marker_scale))
            or float(self.marker_scale) <= 0.0
        ):
            raise ValueError("marker_scale must be a positive finite number")
        object.__setattr__(self, "marker_scale", float(self.marker_scale))


@dataclass(frozen=True)
class TreatmentCatalog:
    """Default treatment plus casefold-keyed per-class overrides.

    When two or more non-default class entries are present, each pair must not
    be color-only: they need different ``stroke_pattern`` and/or at least one
    non-``none`` marker.
    """

    default: RoleTreatment
    by_class_name: Mapping[str, RoleTreatment]

    def __post_init__(self) -> None:
        if not isinstance(self.default, RoleTreatment):
            raise ValueError("default must be a RoleTreatment")
        if not isinstance(self.by_class_name, Mapping):
            raise ValueError("by_class_name must be a mapping")

        normalised: dict[str, RoleTreatment] = {}
        for raw_key, treatment in self.by_class_name.items():
            if not isinstance(raw_key, str) or not raw_key.strip():
                raise ValueError("by_class_name keys must be non-empty strings")
            if not isinstance(treatment, RoleTreatment):
                raise ValueError(f"by_class_name[{raw_key!r}] must be a RoleTreatment")
            key = raw_key.strip().casefold()
            if key in normalised:
                raise ValueError(f"duplicate by_class_name key after casefold: {key!r}")
            normalised[key] = treatment

        object.__setattr__(self, "by_class_name", MappingProxyType(normalised))
        _validate_multi_signifier(normalised)

    def resolve(self, class_name: str) -> RoleTreatment:
        """Return the treatment for ``class_name``, falling back to default."""
        if not isinstance(class_name, str) or not class_name.strip():
            return self.default
        return self.by_class_name.get(class_name.strip().casefold(), self.default)

    def resolve_key(self, class_name: str) -> str:
        """Return the catalog key used for ``class_name`` (or ``'default'``)."""
        if not isinstance(class_name, str) or not class_name.strip():
            return "default"
        key = class_name.strip().casefold()
        if key in self.by_class_name:
            return key
        return "default"


def _validate_multi_signifier(
    by_class_name: Mapping[str, RoleTreatment],
) -> None:
    """Fail when two non-default roles are distinguished by color alone."""
    entries = list(by_class_name.items())
    if len(entries) < 2:
        return
    for index, (name_a, treatment_a) in enumerate(entries):
        for name_b, treatment_b in entries[index + 1 :]:
            patterns_differ = treatment_a.style.stroke_pattern != treatment_b.style.stroke_pattern
            has_marker = treatment_a.marker != "none" or treatment_b.marker != "none"
            if patterns_differ or has_marker:
                continue
            raise ValueError(
                f"roles {name_a!r} and {name_b!r} are color-only; "
                "require different stroke_pattern or a non-none marker "
                "for multi-signifier discrimination"
            )


def load_treatment_catalog(path: str | Path) -> TreatmentCatalog:
    """Load and validate a role treatment catalog from local JSON."""
    catalog_path = _regular_json_file(path)
    try:
        raw = json.loads(catalog_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"could not read role catalog: {error}") from error
    if not isinstance(raw, dict):
        raise ValueError("role catalog root must be a JSON object")
    return treatment_catalog_from_mapping(raw)


def treatment_catalog_from_mapping(raw: Mapping[str, object]) -> TreatmentCatalog:
    """Build a ``TreatmentCatalog`` from a JSON-compatible mapping."""
    allowed = frozenset({"schema_version", "default", "by_class_name"})
    keys = frozenset(raw)
    missing = sorted({"schema_version", "default"} - keys)
    unknown = sorted(keys - allowed)
    if missing or unknown:
        raise ValueError(
            f"role catalog keys do not match schema; missing={missing}, unknown={unknown}"
        )
    version = raw["schema_version"]
    if (
        isinstance(version, bool)
        or not isinstance(version, int)
        or version != ROLE_CATALOG_SCHEMA_VERSION
    ):
        raise ValueError(f"role catalog schema_version must be {ROLE_CATALOG_SCHEMA_VERSION}")

    default_raw = raw["default"]
    if not isinstance(default_raw, Mapping):
        raise ValueError("role catalog default must be an object")
    default = _treatment_from_mapping(default_raw, base_style=OutlineStyle())

    by_raw = raw.get("by_class_name", {})
    if by_raw is None:
        by_raw = {}
    if not isinstance(by_raw, Mapping):
        raise ValueError("role catalog by_class_name must be an object")
    by_class: dict[str, RoleTreatment] = {}
    for class_name, treatment_raw in by_raw.items():
        if not isinstance(class_name, str) or not class_name.strip():
            raise ValueError("by_class_name keys must be non-empty strings")
        if not isinstance(treatment_raw, Mapping):
            raise ValueError(f"by_class_name[{class_name!r}] must be an object")
        # Class entries inherit unset style fields from catalog default.
        by_class[class_name] = _treatment_from_mapping(treatment_raw, base_style=default.style)
    return TreatmentCatalog(default=default, by_class_name=by_class)


def _treatment_from_mapping(
    raw: Mapping[str, object],
    *,
    base_style: OutlineStyle,
) -> RoleTreatment:
    allowed = frozenset({"style", "marker", "marker_scale"})
    unknown = sorted(frozenset(raw) - allowed)
    if unknown:
        raise ValueError(f"role treatment has unknown keys: {unknown}")

    style = base_style
    if "style" in raw:
        style_raw = raw["style"]
        if style_raw is None:
            style = base_style
        elif not isinstance(style_raw, Mapping):
            raise ValueError("role treatment style must be an object")
        else:
            style = _style_from_partial(style_raw, base=base_style)

    marker: RoleMarker = "none"
    if "marker" in raw and raw["marker"] is not None:
        marker_value = raw["marker"]
        if not isinstance(marker_value, str):
            raise ValueError("marker must be a string")
        marker_key = marker_value.strip().casefold()
        if marker_key not in ROLE_MARKERS:
            choices = ", ".join(sorted(ROLE_MARKERS))
            raise ValueError(f"marker must be one of: {choices}; got {marker_value!r}")
        marker = cast(RoleMarker, marker_key)

    marker_scale = 1.0
    if "marker_scale" in raw and raw["marker_scale"] is not None:
        scale_raw = raw["marker_scale"]
        if isinstance(scale_raw, bool) or not isinstance(scale_raw, (int, float)):
            raise ValueError("marker_scale must be a number")
        marker_scale = float(scale_raw)

    return RoleTreatment(style=style, marker=marker, marker_scale=marker_scale)


def _style_from_partial(
    raw: Mapping[str, object],
    *,
    base: OutlineStyle,
) -> OutlineStyle:
    unknown = sorted(frozenset(raw) - _STYLE_JSON_KEYS)
    if unknown:
        raise ValueError(f"role style has unknown keys: {unknown}")

    def pick(key: str, current: object) -> object:
        if key not in raw or raw[key] is None:
            return current
        return raw[key]

    kwargs = {
        "inner_color": pick("inner_color", base.inner_color),
        "outer_color": pick("outer_color", base.outer_color),
        "inner_width": pick("inner_width", base.inner_width),
        "outer_width": pick("outer_width", base.outer_width),
        "fill_opacity": pick("fill_opacity", base.fill_opacity),
        "scale_with_frame": pick("scale_with_frame", base.scale_with_frame),
        "stroke_pattern": pick("stroke_pattern", base.stroke_pattern),
        "dash_period_px": pick("dash_period_px", base.dash_period_px),
        "fill_mode": pick("fill_mode", base.fill_mode),
        "halo_blur": pick("halo_blur", base.halo_blur),
        "adapt_width_to_area": pick("adapt_width_to_area", base.adapt_width_to_area),
        "outline_kernel": pick("outline_kernel", base.outline_kernel),
    }

    def integer(key: str) -> int:
        value = kwargs[key]
        if isinstance(value, bool) or not isinstance(value, (int, float, str)):
            raise ValueError(f"{key} must be an integer-compatible value")
        return int(value)

    try:
        return OutlineStyle(
            inner_color=str(kwargs["inner_color"]),
            outer_color=str(kwargs["outer_color"]),
            inner_width=integer("inner_width"),
            outer_width=integer("outer_width"),
            fill_opacity=float(kwargs["fill_opacity"]),  # type: ignore[arg-type]
            scale_with_frame=bool(kwargs["scale_with_frame"]),
            stroke_pattern=str(kwargs["stroke_pattern"]),
            dash_period_px=integer("dash_period_px"),
            fill_mode=str(kwargs["fill_mode"]),
            halo_blur=integer("halo_blur"),
            adapt_width_to_area=bool(kwargs["adapt_width_to_area"]),
            outline_kernel=str(kwargs["outline_kernel"]),
        )
    except (TypeError, ValueError) as error:
        raise ValueError(f"invalid role style: {error}") from error


def _regular_json_file(path: str | Path) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise ValueError("role catalog path must not be a symlink")
    if candidate.parent.is_symlink():
        raise ValueError("role catalog parent must not be a symlink")
    if not candidate.is_file():
        raise ValueError(f"role catalog is not a regular file: {candidate}")
    if candidate.suffix.lower() != ".json":
        raise ValueError("role catalog must use the .json format")
    return candidate.resolve()


def marker_polygon_px(
    marker: RoleMarker,
    center_x: float,
    center_y: float,
    size: float,
) -> np.ndarray:
    """Return an open polygon (N, 2) in pixel space for a role marker.

    Shapes are closed by the caller via ``fillPoly`` / closed polylines.
    ``size`` is the approximate half-extent in pixels.
    """
    if marker == "none":
        return np.zeros((0, 2), dtype=np.float64)
    half = max(1.0, float(size))
    cx = float(center_x)
    cy = float(center_y)
    if marker == "triangle":
        # Point-up triangle centered at (cx, cy).
        return np.array(
            [
                [cx, cy - half],
                [cx + half, cy + half * 0.75],
                [cx - half, cy + half * 0.75],
            ],
            dtype=np.float64,
        )
    if marker == "square":
        return np.array(
            [
                [cx - half, cy - half],
                [cx + half, cy - half],
                [cx + half, cy + half],
                [cx - half, cy + half],
            ],
            dtype=np.float64,
        )
    if marker == "chevron":
        # Upward chevron (arrowhead-like) as a closed diamond wedge.
        return np.array(
            [
                [cx, cy - half],
                [cx + half, cy],
                [cx, cy - half * 0.25],
                [cx - half, cy],
            ],
            dtype=np.float64,
        )
    raise ValueError(f"unsupported marker: {marker!r}")


def marker_anchor_px(points: np.ndarray) -> tuple[float, float]:
    """Top-center of the axis-aligned bbox of polygon points (pixel space)."""
    coords = np.asarray(points, dtype=np.float64).reshape((-1, 2))
    if coords.shape[0] == 0:
        return 0.0, 0.0
    min_x = float(np.min(coords[:, 0]))
    max_x = float(np.max(coords[:, 0]))
    min_y = float(np.min(coords[:, 1]))
    return (min_x + max_x) * 0.5, min_y


def draw_role_marker(
    cv2: Any,
    output: np.ndarray,
    points: np.ndarray,
    treatment: RoleTreatment,
    *,
    frame_height: int,
) -> None:
    """Draw a dual-contrast closed marker at the top of the instance bbox."""
    if treatment.marker == "none":
        return
    style = treatment.style
    scale = max(1.0, frame_height / 720.0) if style.scale_with_frame else 1.0
    size = max(4.0, 8.0 * float(treatment.marker_scale) * scale)
    anchor_x, anchor_y = marker_anchor_px(points)
    # Sit just above the top of the bbox so the outline is not fully covered.
    center_y = anchor_y - size * 0.85
    poly = marker_polygon_px(treatment.marker, anchor_x, center_y, size)
    if poly.shape[0] < 3:
        return
    fixed = np.rint(poly * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
    outer_bgr = parse_hex_bgr(style.outer_color)
    inner_bgr = parse_hex_bgr(style.inner_color)
    fill_poly = cv2.fillPoly
    polylines = cv2.polylines
    line_aa = cv2.LINE_AA
    # Filled outer, then slightly inset filled inner for dual-contrast disc.
    fill_poly(
        output,
        [fixed],
        outer_bgr,
        shift=DRAW_COORDINATE_SHIFT,
    )
    inset = max(1.0, size * 0.35)
    inner_poly = marker_polygon_px(treatment.marker, anchor_x, center_y, max(1.0, size - inset))
    inner_fixed = np.rint(inner_poly * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
    fill_poly(
        output,
        [inner_fixed],
        inner_bgr,
        shift=DRAW_COORDINATE_SHIFT,
    )
    # Thin outer edge for definition on busy backgrounds.
    polylines(
        output,
        [fixed],
        True,
        outer_bgr,
        max(1, int(round(2 * scale))),
        lineType=line_aa,
        shift=DRAW_COORDINATE_SHIFT,
    )
