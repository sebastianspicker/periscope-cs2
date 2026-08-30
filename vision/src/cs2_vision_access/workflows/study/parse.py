"""Load and parse study packages; orchestrate clip/condition/inference parsers."""

from __future__ import annotations

import json
from pathlib import Path

from cs2_vision_access.workflows.study._util import (
    _optional_text,
    _path_segment,
    _regular_json_file,
)
from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import (
    SCHEMA_VERSION,
    StudyClip,
    StudyCondition,
    StudyInference,
    StudyPackage,
)
from cs2_vision_access.workflows.study.parse_clip import _parse_clip
from cs2_vision_access.workflows.study.parse_condition import (
    _parse_condition,
    resolve_condition_style,
)
from cs2_vision_access.workflows.study.parse_inference import _parse_inference

_STUDY_REQUIRED = frozenset({"schema_version", "study_id", "clips", "conditions"})
_STUDY_OPTIONAL = frozenset({"title", "description", "inference"})
_STUDY_ALLOWED = _STUDY_REQUIRED | _STUDY_OPTIONAL

# Re-export for internal callers (render) and package public surface.
__all__ = [
    "load_study_package",
    "parse_study_package",
    "resolve_condition_style",
]


def load_study_package(path: str | Path) -> StudyPackage:
    """Load and validate a study package JSON from a local regular file."""
    study_path = _regular_json_file(path, label="study package")
    try:
        raw = json.loads(study_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise StudyError(f"could not read study package: {error}") from error
    package = parse_study_package(raw)
    return StudyPackage(
        schema_version=package.schema_version,
        study_id=package.study_id,
        clips=package.clips,
        conditions=package.conditions,
        inference=package.inference,
        title=package.title,
        description=package.description,
        source_path=study_path,
    )


def parse_study_package(raw: object) -> StudyPackage:
    """Validate a study package mapping (no filesystem path resolution)."""
    if not isinstance(raw, dict):
        raise StudyError("study package root must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_STUDY_REQUIRED - keys)
    unknown = sorted(keys - _STUDY_ALLOWED)
    if missing or unknown:
        raise StudyError(
            f"study package keys do not match schema; missing={missing}, unknown={unknown}"
        )

    schema_version = raw["schema_version"]
    if (
        isinstance(schema_version, bool)
        or not isinstance(schema_version, int)
        or schema_version != SCHEMA_VERSION
    ):
        raise StudyError(f"schema_version must be {SCHEMA_VERSION}")

    study_id = _path_segment(raw["study_id"], label="study_id")
    title = _optional_text(raw.get("title"), "title")
    description = _optional_text(raw.get("description"), "description")

    clips_raw = raw["clips"]
    if not isinstance(clips_raw, list) or not clips_raw:
        raise StudyError("clips must be a non-empty JSON array")
    clips: list[StudyClip] = []
    seen_clips: set[str] = set()
    for index, item in enumerate(clips_raw):
        clip = _parse_clip(item, index=index)
        if clip.clip_id in seen_clips:
            raise StudyError(f"duplicate clip_id: {clip.clip_id!r}")
        seen_clips.add(clip.clip_id)
        clips.append(clip)

    conditions_raw = raw["conditions"]
    if not isinstance(conditions_raw, list) or not conditions_raw:
        raise StudyError("conditions must be a non-empty JSON array")
    conditions: list[StudyCondition] = []
    seen_conditions: set[str] = set()
    for index, item in enumerate(conditions_raw):
        condition = _parse_condition(item, index=index)
        if condition.condition_id in seen_conditions:
            raise StudyError(f"duplicate condition_id: {condition.condition_id!r}")
        seen_conditions.add(condition.condition_id)
        conditions.append(condition)

    inference: StudyInference | None = None
    if "inference" in raw and raw["inference"] is not None:
        inference = _parse_inference(raw["inference"])

    has_outline = any(condition.kind == "outline" for condition in conditions)
    if has_outline and inference is None:
        raise StudyError("inference is required when any condition has kind=outline")

    # Resolve outline styles immediately so invalid contrast/presets fail closed
    # at load time (paths are not required yet for style geometry).
    for condition in conditions:
        if condition.kind == "outline":
            resolve_condition_style(condition, prefs_loader=None)

    return StudyPackage(
        schema_version=schema_version,
        study_id=study_id,
        clips=tuple(clips),
        conditions=tuple(conditions),
        inference=inference,
        title=title,
        description=description,
        source_path=None,
    )
