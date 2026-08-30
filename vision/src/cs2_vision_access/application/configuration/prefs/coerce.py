"""Type-coercion helpers for outline preference fields."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.application.configuration.prefs.errors import OutlinePreferencesError
from cs2_vision_access.domain.outline import (
    FILL_MODES,
    OUTLINE_KERNELS,
    STROKE_PATTERNS,
)


def _from_mapping(raw: dict[str, Any]) -> dict[str, object]:
    """Normalize a JSON object without importing the preferences model."""
    return {
        "schema_version": raw["schema_version"],
        "preset": _required_text(raw["preset"], "preset"),
        "inner_color": _optional_color(raw.get("inner_color"), "inner_color"),
        "outer_color": _optional_color(raw.get("outer_color"), "outer_color"),
        "inner_width": _optional_int(raw.get("inner_width"), "inner_width"),
        "outer_width": _optional_int(raw.get("outer_width"), "outer_width"),
        "fill_opacity": _optional_float(raw.get("fill_opacity"), "fill_opacity"),
        "scale_with_frame": _optional_bool(raw.get("scale_with_frame"), "scale_with_frame"),
        "stroke_pattern": _optional_stroke_pattern(raw.get("stroke_pattern"), "stroke_pattern"),
        "dash_period_px": _optional_int(raw.get("dash_period_px"), "dash_period_px"),
        "fill_mode": _optional_fill_mode(raw.get("fill_mode"), "fill_mode"),
        "halo_blur": _optional_int(raw.get("halo_blur"), "halo_blur"),
        "adapt_width_to_area": _optional_bool(
            raw.get("adapt_width_to_area"), "adapt_width_to_area"
        ),
        "outline_kernel": _optional_outline_kernel(raw.get("outline_kernel"), "outline_kernel"),
    }


def _required_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise OutlinePreferencesError(f"{field} must be a non-empty string")
    if "\x00" in value:
        raise OutlinePreferencesError(f"{field} must not contain NUL")
    return value.strip()


def _optional_color(value: object, field: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise OutlinePreferencesError(f"{field} must be a #RRGGBB string or null")
    return value


def _optional_int(value: object, field: str) -> int | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int):
        raise OutlinePreferencesError(f"{field} must be an integer or null")
    return value


def _optional_float(value: object, field: str) -> float | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise OutlinePreferencesError(f"{field} must be a number or null")
    return float(value)


def _optional_bool(value: object, field: str) -> bool | None:
    if value is None:
        return None
    if not isinstance(value, bool):
        raise OutlinePreferencesError(f"{field} must be a boolean or null")
    return value


def _optional_stroke_pattern(value: object, field: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str) or not value.strip():
        raise OutlinePreferencesError(f"{field} must be a non-empty string or null")
    pattern = value.strip()
    if pattern not in STROKE_PATTERNS:
        choices = ", ".join(sorted(STROKE_PATTERNS))
        raise OutlinePreferencesError(f"{field} must be one of: {choices}; got {pattern!r}")
    return pattern


def _optional_fill_mode(value: object, field: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str) or not value.strip():
        raise OutlinePreferencesError(f"{field} must be a non-empty string or null")
    mode = value.strip()
    if mode not in FILL_MODES:
        choices = ", ".join(sorted(FILL_MODES))
        raise OutlinePreferencesError(f"{field} must be one of: {choices}; got {mode!r}")
    return mode


def _optional_outline_kernel(value: object, field: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str) or not value.strip():
        raise OutlinePreferencesError(f"{field} must be a non-empty string or null")
    kernel = value.strip()
    if kernel not in OUTLINE_KERNELS:
        choices = ", ".join(sorted(OUTLINE_KERNELS))
        raise OutlinePreferencesError(f"{field} must be one of: {choices}; got {kernel!r}")
    return kernel
