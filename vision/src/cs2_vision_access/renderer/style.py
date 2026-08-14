"""Outline style, diagnostics, presets, contrast, and effective widths."""

from __future__ import annotations

import math
import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass

import numpy as np

_HEX_COLOR = re.compile(r"^#[0-9a-fA-F]{6}$")
MINIMUM_STROKE_CONTRAST = 3.0
REFERENCE_FRAME_HEIGHT = 720
DRAW_COORDINATE_SHIFT = 4
DRAW_COORDINATE_SCALE = 1 << DRAW_COORDINATE_SHIFT
STROKE_PATTERNS = frozenset({"solid", "dashed", "dotted"})
FILL_MODES = frozenset({"tint", "halo"})
OUTLINE_KERNELS = frozenset({"polyline", "distance", "jfa"})
DEFAULT_DASH_PERIOD_PX = 12
DEFAULT_HALO_BLUR = 9
# Adaptive width: scale factor from polygon area vs frame area, clamped.
AREA_WIDTH_SCALE_MIN = 0.75
AREA_WIDTH_SCALE_MAX = 2.5
# Area fraction treated as "typical" player footprint → scale ≈ 1.0.
REFERENCE_AREA_FRACTION = 0.01


@dataclass(frozen=True)
class OutlineStyle:
    """Static, configurable outline with dark outer and bright inner strokes.

    ``stroke_pattern`` is a second, non-color channel (solid / dashed / dotted).
    Patterns are geometric along the contour only — no temporal phase, animation,
    or frame-to-frame dash state.

    ``fill_mode`` selects hard interior tint (default) or a soft static halo glow.
    ``outline_kernel`` selects polyline dual-stroke (default), OpenCV distance-transform
    edge bands, or multi-step Jump Flood (JFA) distance bands for thick dual outlines.
    All paths are deterministic and free of temporal state.
    """

    inner_color: str = "#F6FF00"
    outer_color: str = "#101010"
    inner_width: int = 3
    outer_width: int = 7
    fill_opacity: float = 0.08
    scale_with_frame: bool = True
    stroke_pattern: str = "solid"
    dash_period_px: int = DEFAULT_DASH_PERIOD_PX
    fill_mode: str = "tint"
    halo_blur: int = DEFAULT_HALO_BLUR
    adapt_width_to_area: bool = False
    outline_kernel: str = "polyline"

    def __post_init__(self) -> None:
        parse_hex_bgr(self.inner_color)
        parse_hex_bgr(self.outer_color)
        if contrast_ratio(self.inner_color, self.outer_color) < MINIMUM_STROKE_CONTRAST:
            raise ValueError(
                "inner and outer colors must have a contrast ratio of at least "
                f"{MINIMUM_STROKE_CONTRAST}:1"
            )
        if self.inner_width <= 0:
            raise ValueError("inner_width must be positive")
        if self.outer_width < self.inner_width + 2:
            raise ValueError("outer_width must be at least inner_width + 2")
        if not 0.0 <= self.fill_opacity <= 0.35:
            raise ValueError("fill_opacity must be in [0, 0.35]")
        if not isinstance(self.scale_with_frame, bool):
            raise ValueError("scale_with_frame must be a boolean")
        if self.stroke_pattern not in STROKE_PATTERNS:
            choices = ", ".join(sorted(STROKE_PATTERNS))
            raise ValueError(
                f"stroke_pattern must be one of: {choices}; got {self.stroke_pattern!r}"
            )
        if (
            isinstance(self.dash_period_px, bool)
            or not isinstance(self.dash_period_px, int)
            or self.dash_period_px <= 0
        ):
            raise ValueError("dash_period_px must be a positive integer")
        if self.fill_mode not in FILL_MODES:
            choices = ", ".join(sorted(FILL_MODES))
            raise ValueError(f"fill_mode must be one of: {choices}; got {self.fill_mode!r}")
        if (
            isinstance(self.halo_blur, bool)
            or not isinstance(self.halo_blur, int)
            or self.halo_blur <= 0
        ):
            raise ValueError("halo_blur must be a positive integer")
        if not isinstance(self.adapt_width_to_area, bool):
            raise ValueError("adapt_width_to_area must be a boolean")
        if self.outline_kernel not in OUTLINE_KERNELS:
            choices = ", ".join(sorted(OUTLINE_KERNELS))
            raise ValueError(
                f"outline_kernel must be one of: {choices}; got {self.outline_kernel!r}"
            )


@dataclass(frozen=True)
class RenderDiagnostics:
    """Exact per-frame rendering decisions for status and regression evidence."""

    predictions_received: int
    current_predictions: int
    stale_predictions_discarded: int
    degenerate_masks_discarded: int
    contours_rendered: int
    inner_width_pixels: int
    outer_width_pixels: int
    stroke_contrast_ratio: float
    stroke_pattern: str
    dash_period_pixels: int
    # When a TreatmentCatalog is active: counts of contours by resolved role key
    # (class casefold name, or "default"). None preserves single-style path.
    roles_applied: Mapping[str, int] | None = None


def parse_hex_bgr(value: str) -> tuple[int, int, int]:
    """Convert #RRGGBB to OpenCV's BGR ordering."""
    if not isinstance(value, str) or not _HEX_COLOR.fullmatch(value):
        raise ValueError("colors must use #RRGGBB notation")
    red = int(value[1:3], 16)
    green = int(value[3:5], 16)
    blue = int(value[5:7], 16)
    return blue, green, red


def contrast_ratio(first: str, second: str) -> float:
    """Return the WCAG relative-luminance contrast ratio for two sRGB colors."""
    first_luminance = _relative_luminance(first)
    second_luminance = _relative_luminance(second)
    lighter = max(first_luminance, second_luminance)
    darker = min(first_luminance, second_luminance)
    return (lighter + 0.05) / (darker + 0.05)


def get_outline_preset(name: str) -> OutlineStyle:
    try:
        return OUTLINE_PRESETS[name]
    except KeyError as error:
        choices = ", ".join(sorted(OUTLINE_PRESETS))
        raise ValueError(f"unknown outline preset {name!r}; choose one of: {choices}") from error


def effective_line_widths(style: OutlineStyle, frame_height: int) -> tuple[int, int]:
    """Scale line widths at high resolutions while preserving the two-stroke boundary."""
    if frame_height <= 0:
        raise ValueError("frame_height must be positive")
    scale = max(1.0, frame_height / REFERENCE_FRAME_HEIGHT) if style.scale_with_frame else 1.0
    inner = max(1, math.floor(style.inner_width * scale + 0.5))
    outer = max(inner + 2, math.floor(style.outer_width * scale + 0.5))
    return inner, outer


def effective_instance_line_widths(
    style: OutlineStyle,
    frame_height: int,
    frame_width: int,
    polygon: Sequence[Sequence[float]] | np.ndarray,
) -> tuple[int, int]:
    """Return per-instance dual-stroke widths, optionally area-adapted.

    When ``adapt_width_to_area`` is False, matches ``effective_line_widths``.
    When True, scales widths from polygon area / frame area so smaller players
    receive thicker relative strokes. Scale factor is clamped to
    [``AREA_WIDTH_SCALE_MIN``, ``AREA_WIDTH_SCALE_MAX``] and outer stays
    ≥ inner + 2.
    """
    base_inner, base_outer = effective_line_widths(style, frame_height)
    if not style.adapt_width_to_area:
        return base_inner, base_outer
    if frame_width <= 0:
        raise ValueError("frame_width must be positive")
    frame_area = float(frame_height * frame_width)
    poly_area = abs(_polygon_area(polygon))
    area_frac = poly_area / frame_area if frame_area > 0 else 0.0
    if area_frac <= 0.0:
        scale = AREA_WIDTH_SCALE_MAX
    else:
        scale = math.sqrt(REFERENCE_AREA_FRACTION / area_frac)
    scale = min(AREA_WIDTH_SCALE_MAX, max(AREA_WIDTH_SCALE_MIN, scale))
    inner = max(1, math.floor(base_inner * scale + 0.5))
    outer = max(inner + 2, math.floor(base_outer * scale + 0.5))
    return inner, outer


def effective_dash_period(style: OutlineStyle, frame_height: int) -> int:
    """Scale dash/dot period with frame height the same way line widths scale."""
    if frame_height <= 0:
        raise ValueError("frame_height must be positive")
    scale = max(1.0, frame_height / REFERENCE_FRAME_HEIGHT) if style.scale_with_frame else 1.0
    return max(1, math.floor(style.dash_period_px * scale + 0.5))


def _polygon_area(polygon: Sequence[Sequence[float]] | np.ndarray) -> float:
    """Shoelace area of a 2D polygon (pixel space)."""
    points = np.asarray(polygon, dtype=np.float64).reshape((-1, 2))
    if points.shape[0] < 3:
        return 0.0
    shifted = np.roll(points, -1, axis=0)
    return float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)


def _relative_luminance(value: str) -> float:
    blue, green, red = parse_hex_bgr(value)

    def linear(channel: int) -> float:
        normalised = channel / 255.0
        if normalised <= 0.04045:
            return normalised / 12.92
        return ((normalised + 0.055) / 1.055) ** 2.4

    return 0.2126 * linear(red) + 0.7152 * linear(green) + 0.0722 * linear(blue)


OUTLINE_PRESETS = {
    "high-visibility": OutlineStyle(),
    "maximum-visibility": OutlineStyle(
        inner_color="#FFFFFF",
        outer_color="#000000",
        inner_width=5,
        outer_width=13,
        fill_opacity=0.15,
    ),
    "cyan-black": OutlineStyle(
        inner_color="#00F5FF",
        outer_color="#050505",
        inner_width=4,
        outer_width=9,
        fill_opacity=0.10,
    ),
}

OUTLINE_PRESET_DESCRIPTIONS = {
    "high-visibility": (
        "Bright yellow-green with a dark boundary and restrained fill; "
        "default solid dual-stroke (dashed/dotted available via stroke_pattern)."
    ),
    "maximum-visibility": (
        "Thick white-on-black boundary with a stronger interior tint; "
        "solid dual-stroke by default (pattern channel via stroke_pattern)."
    ),
    "cyan-black": (
        "Cyan-on-black alternative that does not depend on red/green contrast; "
        "solid dual-stroke by default (pattern channel via stroke_pattern)."
    ),
}
