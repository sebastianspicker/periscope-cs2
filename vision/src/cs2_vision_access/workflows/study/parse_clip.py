"""Parse study package clip entries."""

from __future__ import annotations

from cs2_vision_access.workflows.study._util import (
    _optional_text,
    _path_segment,
    _required_text,
)
from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import StudyClip

_CLIP_REQUIRED = frozenset({"clip_id", "input"})
_CLIP_OPTIONAL = frozenset({"label", "notes", "tags"})
_CLIP_ALLOWED = _CLIP_REQUIRED | _CLIP_OPTIONAL


def _parse_clip(raw: object, *, index: int) -> StudyClip:
    if not isinstance(raw, dict):
        raise StudyError(f"clips[{index}] must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_CLIP_REQUIRED - keys)
    unknown = sorted(keys - _CLIP_ALLOWED)
    if missing or unknown:
        raise StudyError(
            f"clips[{index}] keys do not match schema; missing={missing}, unknown={unknown}"
        )
    clip_id = _path_segment(raw["clip_id"], label=f"clips[{index}].clip_id")
    input_value = _required_text(raw["input"], f"clips[{index}].input")
    label = _optional_text(raw.get("label"), f"clips[{index}].label")
    notes = _optional_text(raw.get("notes"), f"clips[{index}].notes")
    tags: tuple[str, ...] = ()
    if "tags" in raw and raw["tags"] is not None:
        tags_raw = raw["tags"]
        if not isinstance(tags_raw, list) or not all(
            isinstance(tag, str) and tag.strip() for tag in tags_raw
        ):
            raise StudyError(f"clips[{index}].tags must be an array of non-empty strings")
        tags = tuple(tag.strip() for tag in tags_raw)
    return StudyClip(
        clip_id=clip_id,
        input=input_value,
        label=label,
        notes=notes,
        tags=tags,
    )
