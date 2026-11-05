"""Shared outline-style resolution: preset → prefs file → explicit overrides."""

from __future__ import annotations

from cs2_vision_access.application.configuration.prefs.model import (
    DEFAULT_PRESET,
    OutlinePreferences,
)
from cs2_vision_access.domain.outline import OutlineStyle, get_outline_preset


def _integer(value: object, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        raise ValueError(f"{field} must be an integer-compatible value")
    return int(value)


def _number(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        raise ValueError(f"{field} must be a number")
    return float(value)


def resolve_style_with_preferences(
    preferences: OutlinePreferences | None,
    *,
    preset: str | None = None,
    inner_color: str | None = None,
    outer_color: str | None = None,
    inner_width: int | None = None,
    outer_width: int | None = None,
    fill_opacity: float | None = None,
    scale_with_frame: bool | None = None,
    stroke_pattern: str | None = None,
    dash_period_px: int | None = None,
    fill_mode: str | None = None,
    halo_blur: int | None = None,
    adapt_width_to_area: bool | None = None,
    outline_kernel: str | None = None,
) -> OutlineStyle:
    """Resolve a style; each field takes the override, else prefs, else the preset.

    The preset is the explicit ``preset``, else the prefs preset, else the default.
    Raises ``ValueError`` for unknown presets and invalid values.
    """
    if preset is None:
        preset = DEFAULT_PRESET if preferences is None else preferences.preset
    base = get_outline_preset(preset)

    def _pick(name: str, override: object) -> object:
        if override is not None:
            return override
        prefs_value = None if preferences is None else getattr(preferences, name)
        return prefs_value if prefs_value is not None else getattr(base, name)

    return OutlineStyle(
        inner_color=str(_pick("inner_color", inner_color)),
        outer_color=str(_pick("outer_color", outer_color)),
        inner_width=_integer(_pick("inner_width", inner_width), "inner_width"),
        outer_width=_integer(_pick("outer_width", outer_width), "outer_width"),
        fill_opacity=_number(_pick("fill_opacity", fill_opacity), "fill_opacity"),
        scale_with_frame=bool(_pick("scale_with_frame", scale_with_frame)),
        stroke_pattern=str(_pick("stroke_pattern", stroke_pattern)),
        dash_period_px=_integer(_pick("dash_period_px", dash_period_px), "dash_period_px"),
        fill_mode=str(_pick("fill_mode", fill_mode)),
        halo_blur=_integer(_pick("halo_blur", halo_blur), "halo_blur"),
        adapt_width_to_area=bool(_pick("adapt_width_to_area", adapt_width_to_area)),
        outline_kernel=str(_pick("outline_kernel", outline_kernel)),
    )
