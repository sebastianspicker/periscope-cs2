"""Offline instance enter/leave cue event log (file-only research).

Cue events are a non-visual research channel: they record when a detected
instance appears or disappears across frames of a recorded video. Matching is
centroid proximity within a class; there is no live capture or network I/O.
"""

from __future__ import annotations

from cs2_vision_access.workflows.evaluation.cues.errors import CueLogError
from cs2_vision_access.workflows.evaluation.cues.geometry import mask_centroid
from cs2_vision_access.workflows.evaluation.cues.load import load_cue_events, parse_cue_event
from cs2_vision_access.workflows.evaluation.cues.models import (
    CUE_EVENTS,
    DEFAULT_MATCH_DISTANCE_PX,
    SCHEMA_VERSION,
    CueEvent,
)
from cs2_vision_access.workflows.evaluation.cues.writer import CueLogWriter

__all__ = [
    "SCHEMA_VERSION",
    "CUE_EVENTS",
    "DEFAULT_MATCH_DISTANCE_PX",
    "CueLogError",
    "CueEvent",
    "CueLogWriter",
    "load_cue_events",
    "parse_cue_event",
    "mask_centroid",
]
