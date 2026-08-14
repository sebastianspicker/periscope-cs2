"""Load and parse schema-versioned cue event logs."""

from __future__ import annotations

import json
from pathlib import Path

from cs2_vision_access.cues.errors import CueLogError
from cs2_vision_access.cues.models import _REQUIRED_KEYS, CueEvent
from cs2_vision_access.cues.paths import _regular_cue_file


def load_cue_events(path: str | Path) -> tuple[CueEvent, ...]:
    """Load and validate a local JSONL cue log."""
    cue_path = _regular_cue_file(path)
    try:
        text = cue_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise CueLogError(f"could not read cue log: {error}") from error
    events: list[CueEvent] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            raw = json.loads(stripped)
        except json.JSONDecodeError as error:
            raise CueLogError(f"cue log line {line_number} is not valid JSON: {error}") from error
        try:
            events.append(parse_cue_event(raw))
        except CueLogError as error:
            raise CueLogError(f"cue log line {line_number}: {error}") from error
    return tuple(events)


def parse_cue_event(raw: object) -> CueEvent:
    """Parse one JSON object into a validated ``CueEvent`` (unknown keys fail)."""
    if not isinstance(raw, dict):
        raise CueLogError("cue event root must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_REQUIRED_KEYS - keys)
    unknown = sorted(keys - _REQUIRED_KEYS)
    if missing or unknown:
        raise CueLogError(
            f"cue event keys do not match schema; missing={missing}, unknown={unknown}"
        )
    return CueEvent(
        schema_version=raw["schema_version"],
        event=raw["event"],
        frame_index=raw["frame_index"],
        track_id=raw["track_id"],
        class_id=raw["class_id"],
        class_name=raw["class_name"],
        confidence=raw["confidence"],
        centroid_x=raw["centroid_x"],
        centroid_y=raw["centroid_y"],
    )
