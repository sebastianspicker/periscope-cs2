"""Parse study package conditions and resolve outline styles."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.prefs import (
    DEFAULT_PRESET,
    OutlinePreferences,
    load_outline_preferences,
)
from cs2_vision_access.renderer import (
    OUTLINE_PRESETS,
    STROKE_PATTERNS,
    OutlineStyle,
    get_outline_preset,
)
from cs2_vision_access.study._util import (
    _optional_bool,
    _optional_color,
    _optional_float,
    _optional_int,
    _optional_text,
    _path_segment,
    _required_text,
    _resolve_existing_path,
)
from cs2_vision_access.study.errors import StudyError
from cs2_vision_access.study.models import CONDITION_KINDS, StudyCondition

_CONDITION_REQUIRED = frozenset({"condition_id", "kind"})
_CONDITION_OPTIONAL = frozenset(
    {
        "label",
        "description",
        "outline_preset",
        "prefs",
        "inner_color",
        "outer_color",
        "inner_width",
        "outer_width",
        "fill_opacity",
        "scale_with_frame",
        "stroke_pattern",
        "dash_period_px",
    }
)
_CONDITION_ALLOWED = _CONDITION_REQUIRED | _CONDITION_OPTIONAL
_CONDITION_OUTLINE_KEYS = frozenset(
    {
        "outline_preset",
        "prefs",
        "inner_color",
        "outer_color",
        "inner_width",
        "outer_width",
        "fill_opacity",
        "scale_with_frame",
        "stroke_pattern",
        "dash_period_px",
    }
)


def resolve_condition_style(
    condition: StudyCondition,
    *,
    base_directory: Path | None = None,
    prefs_loader: Callable[[Path], OutlinePreferences] | None = load_outline_preferences,
) -> OutlineStyle:
    """Resolve an outline condition to a validated ``OutlineStyle``.

    Precedence matches the outline CLI: named preset → prefs file → field overrides.
    ``prefs_loader=None`` skips loading prefs paths (schema-only validation of
    non-path overrides); if ``prefs`` is set while loader is None, only the
    preset and field overrides are applied and the prefs path is ignored for
    style resolution (still validated as a non-empty string at parse time).
    """
    if condition.kind != "outline":
        raise StudyError(f"resolve_condition_style requires kind=outline; got {condition.kind!r}")

    preferences: OutlinePreferences | None = None
    if condition.prefs is not None and prefs_loader is not None:
        if base_directory is None:
            raise StudyError("base_directory is required to load condition prefs")
        prefs_path = _resolve_existing_path(
            condition.prefs, base_directory=base_directory, label="prefs"
        )
        try:
            preferences = prefs_loader(prefs_path)
        except Exception as error:
            # prefs module raises OutlinePreferencesError; keep study surface stable.
            raise StudyError(f"could not load condition prefs: {error}") from error

    if condition.outline_preset is not None:
        preset_name = condition.outline_preset
    elif preferences is not None:
        preset_name = preferences.preset
    else:
        preset_name = DEFAULT_PRESET

    try:
        base = get_outline_preset(preset_name)
    except ValueError as error:
        raise StudyError(str(error)) from error

    def _pick(override: object, prefs_value: object, base_value: object) -> object:
        if override is not None:
            return override
        if prefs_value is not None:
            return prefs_value
        return base_value

    prefs_inner = None if preferences is None else preferences.inner_color
    prefs_outer = None if preferences is None else preferences.outer_color
    prefs_inner_width = None if preferences is None else preferences.inner_width
    prefs_outer_width = None if preferences is None else preferences.outer_width
    prefs_fill = None if preferences is None else preferences.fill_opacity
    prefs_scale = None if preferences is None else preferences.scale_with_frame
    prefs_pattern = None if preferences is None else preferences.stroke_pattern
    prefs_period = None if preferences is None else preferences.dash_period_px
    prefs_fill_mode = None if preferences is None else preferences.fill_mode
    prefs_halo_blur = None if preferences is None else preferences.halo_blur
    prefs_adapt = None if preferences is None else preferences.adapt_width_to_area
    prefs_kernel = None if preferences is None else preferences.outline_kernel

    try:
        return OutlineStyle(
            inner_color=str(_pick(condition.inner_color, prefs_inner, base.inner_color)),
            outer_color=str(_pick(condition.outer_color, prefs_outer, base.outer_color)),
            inner_width=int(
                _pick(condition.inner_width, prefs_inner_width, base.inner_width)  # type: ignore[arg-type]
            ),
            outer_width=int(
                _pick(condition.outer_width, prefs_outer_width, base.outer_width)  # type: ignore[arg-type]
            ),
            fill_opacity=float(
                _pick(condition.fill_opacity, prefs_fill, base.fill_opacity)  # type: ignore[arg-type]
            ),
            scale_with_frame=bool(
                _pick(condition.scale_with_frame, prefs_scale, base.scale_with_frame)
            ),
            stroke_pattern=str(_pick(condition.stroke_pattern, prefs_pattern, base.stroke_pattern)),
            dash_period_px=int(
                _pick(condition.dash_period_px, prefs_period, base.dash_period_px)  # type: ignore[arg-type]
            ),
            fill_mode=str(_pick(None, prefs_fill_mode, base.fill_mode)),
            halo_blur=int(_pick(None, prefs_halo_blur, base.halo_blur)),  # type: ignore[arg-type]
            adapt_width_to_area=bool(_pick(None, prefs_adapt, base.adapt_width_to_area)),
            outline_kernel=str(_pick(None, prefs_kernel, base.outline_kernel)),
        )
    except ValueError as error:
        raise StudyError(f"condition {condition.condition_id!r} style invalid: {error}") from error


def _parse_condition(raw: object, *, index: int) -> StudyCondition:
    if not isinstance(raw, dict):
        raise StudyError(f"conditions[{index}] must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_CONDITION_REQUIRED - keys)
    unknown = sorted(keys - _CONDITION_ALLOWED)
    if missing or unknown:
        raise StudyError(
            f"conditions[{index}] keys do not match schema; missing={missing}, unknown={unknown}"
        )
    condition_id = _path_segment(raw["condition_id"], label=f"conditions[{index}].condition_id")
    kind = _required_text(raw["kind"], f"conditions[{index}].kind")
    if kind not in CONDITION_KINDS:
        choices = ", ".join(sorted(CONDITION_KINDS))
        raise StudyError(f"conditions[{index}].kind must be one of: {choices}; got {kind!r}")

    outline_fields_present = any(
        key in raw and raw[key] is not None for key in _CONDITION_OUTLINE_KEYS
    )
    if kind == "baseline" and outline_fields_present:
        raise StudyError(f"conditions[{index}] kind=baseline must not set outline style fields")
    if kind == "outline" and not outline_fields_present:
        # Explicit empty outline is allowed only via default preset; still require
        # at least outline_preset or prefs for operator clarity.
        raise StudyError(
            f"conditions[{index}] kind=outline requires outline_preset, prefs, "
            "or at least one style override"
        )

    outline_preset = _optional_text(
        raw.get("outline_preset"), f"conditions[{index}].outline_preset"
    )
    if outline_preset is not None and outline_preset not in OUTLINE_PRESETS:
        choices = ", ".join(sorted(OUTLINE_PRESETS))
        raise StudyError(
            f"conditions[{index}].outline_preset must be one of: {choices}; got {outline_preset!r}"
        )

    prefs = _optional_text(raw.get("prefs"), f"conditions[{index}].prefs")
    stroke_pattern = _optional_text(
        raw.get("stroke_pattern"), f"conditions[{index}].stroke_pattern"
    )
    if stroke_pattern is not None and stroke_pattern not in STROKE_PATTERNS:
        choices = ", ".join(sorted(STROKE_PATTERNS))
        raise StudyError(
            f"conditions[{index}].stroke_pattern must be one of: {choices}; got {stroke_pattern!r}"
        )

    return StudyCondition(
        condition_id=condition_id,
        kind=kind,
        label=_optional_text(raw.get("label"), f"conditions[{index}].label"),
        description=_optional_text(raw.get("description"), f"conditions[{index}].description"),
        outline_preset=outline_preset,
        prefs=prefs,
        inner_color=_optional_color(raw.get("inner_color"), f"conditions[{index}].inner_color"),
        outer_color=_optional_color(raw.get("outer_color"), f"conditions[{index}].outer_color"),
        inner_width=_optional_int(raw.get("inner_width"), f"conditions[{index}].inner_width"),
        outer_width=_optional_int(raw.get("outer_width"), f"conditions[{index}].outer_width"),
        fill_opacity=_optional_float(raw.get("fill_opacity"), f"conditions[{index}].fill_opacity"),
        scale_with_frame=_optional_bool(
            raw.get("scale_with_frame"), f"conditions[{index}].scale_with_frame"
        ),
        stroke_pattern=stroke_pattern,
        dash_period_px=_optional_int(
            raw.get("dash_period_px"), f"conditions[{index}].dash_period_px"
        ),
    )
