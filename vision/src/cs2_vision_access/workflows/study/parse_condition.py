"""Parse study package conditions and resolve outline styles."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.application.configuration.prefs import (
    OutlinePreferences,
    load_outline_preferences,
)
from cs2_vision_access.application.configuration.prefs.resolve import (
    resolve_style_with_preferences,
)
from cs2_vision_access.domain.outline import (
    OUTLINE_PRESETS,
    STROKE_PATTERNS,
    OutlineStyle,
)
from cs2_vision_access.workflows.study._util import (
    _optional_bool,
    _optional_color,
    _optional_float,
    _optional_int,
    _optional_text,
    _path_segment,
    _required_text,
    _resolve_existing_path,
)
from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import CONDITION_KINDS, StudyCondition

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

    try:
        return resolve_style_with_preferences(
            preferences,
            preset=condition.outline_preset,
            inner_color=condition.inner_color,
            outer_color=condition.outer_color,
            inner_width=condition.inner_width,
            outer_width=condition.outer_width,
            fill_opacity=condition.fill_opacity,
            scale_with_frame=condition.scale_with_frame,
            stroke_pattern=condition.stroke_pattern,
            dash_period_px=condition.dash_period_px,
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
