"""Cue event models and package constants."""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass

from cs2_vision_access.cues.errors import CueLogError
from cs2_vision_access.cues.geometry import _finite_float

SCHEMA_VERSION = 1
CUE_EVENTS = frozenset({"enter", "leave"})
DEFAULT_MATCH_DISTANCE_PX = 64.0
_REQUIRED_KEYS = frozenset(
    {
        "schema_version",
        "event",
        "frame_index",
        "track_id",
        "class_id",
        "class_name",
        "confidence",
        "centroid_x",
        "centroid_y",
    }
)


@dataclass(frozen=True)
class CueEvent:
    """One enter or leave observation for an ephemeral offline track."""

    schema_version: int
    event: str
    frame_index: int
    track_id: int
    class_id: int
    class_name: str
    confidence: float
    centroid_x: float
    centroid_y: float

    def __post_init__(self) -> None:
        if (
            isinstance(self.schema_version, bool)
            or not isinstance(self.schema_version, int)
            or self.schema_version != SCHEMA_VERSION
        ):
            raise CueLogError(f"schema_version must be {SCHEMA_VERSION}")
        if self.event not in CUE_EVENTS:
            choices = ", ".join(sorted(CUE_EVENTS))
            raise CueLogError(f"event must be one of: {choices}; got {self.event!r}")
        if (
            isinstance(self.frame_index, bool)
            or not isinstance(self.frame_index, int)
            or self.frame_index < 0
        ):
            raise CueLogError("frame_index must be a non-negative integer")
        if (
            isinstance(self.track_id, bool)
            or not isinstance(self.track_id, int)
            or self.track_id < 0
        ):
            raise CueLogError("track_id must be a non-negative integer")
        if (
            isinstance(self.class_id, bool)
            or not isinstance(self.class_id, int)
            or self.class_id < 0
        ):
            raise CueLogError("class_id must be a non-negative integer")
        if not isinstance(self.class_name, str) or not self.class_name.strip():
            raise CueLogError("class_name must be a non-empty string")
        if isinstance(self.confidence, bool) or not isinstance(self.confidence, (int, float)):
            raise CueLogError("confidence must be a number")
        confidence = float(self.confidence)
        if not math.isfinite(confidence) or not 0.0 <= confidence <= 1.0:
            raise CueLogError("confidence must be a finite value in [0, 1]")
        object.__setattr__(self, "confidence", confidence)
        object.__setattr__(self, "class_name", self.class_name.strip())
        object.__setattr__(self, "centroid_x", _finite_float(self.centroid_x, "centroid_x"))
        object.__setattr__(self, "centroid_y", _finite_float(self.centroid_y, "centroid_y"))

    def as_json(self) -> dict[str, object]:
        """Serialize to a deterministic JSON-ready mapping."""
        return asdict(self)


@dataclass
class _ActiveTrack:
    track_id: int
    class_id: int
    class_name: str
    confidence: float
    centroid_x: float
    centroid_y: float
    last_frame_index: int
