"""Write schema-versioned JSONL enter/leave cue events."""

from __future__ import annotations

import json
import math
import os
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import TypedDict, cast

from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.workflows.evaluation.cues.errors import CueLogError
from cs2_vision_access.workflows.evaluation.cues.geometry import _observation_from_mask
from cs2_vision_access.workflows.evaluation.cues.models import (
    DEFAULT_MATCH_DISTANCE_PX,
    SCHEMA_VERSION,
    CueEvent,
    _ActiveTrack,
)
from cs2_vision_access.workflows.evaluation.cues.paths import _prepare_cue_log_path


class _Observation(TypedDict):
    class_id: int
    class_name: str
    confidence: float
    centroid_x: float
    centroid_y: float


class CueLogWriter:
    """Write schema-versioned JSONL enter/leave events for offline research.

    Events stream to a same-directory partial file
    (``.{stem}.partial{suffix}``). ``close(commit=True)`` (the default)
    atomically ``os.replace``s the partial onto the destination; ``commit=False``
    discards the partial and leaves any pre-existing destination untouched.
    """

    def __init__(
        self,
        path: str | Path,
        *,
        match_distance_px: float = DEFAULT_MATCH_DISTANCE_PX,
        overwrite: bool = False,
    ) -> None:
        if (
            isinstance(match_distance_px, bool)
            or not isinstance(match_distance_px, (int, float))
            or not math.isfinite(match_distance_px)
            or match_distance_px <= 0
        ):
            raise CueLogError("match_distance_px must be a finite positive number")
        self._match_distance_px = float(match_distance_px)
        self._path = _prepare_cue_log_path(path, overwrite=overwrite)
        self._partial = self._path.with_name(f".{self._path.stem}.partial{self._path.suffix}")
        if self._partial.is_symlink():
            raise CueLogError("cue log partial path must not be a symlink")
        if self._partial.exists():
            raise CueLogError(
                f"stale partial cue log exists: {self._partial}; remove it explicitly"
            )
        self._handle = self._partial.open("w", encoding="utf-8")
        self._active: list[_ActiveTrack] = []
        self._next_track_id = 0
        self._events_written = 0
        self._last_frame_index = -1
        self._closed = False
        self._committed = False

    @property
    def path(self) -> Path:
        """Final destination path (committed only after successful close)."""
        return self._path

    @property
    def partial_path(self) -> Path:
        """In-progress partial path used until a successful commit."""
        return self._partial

    @property
    def events_written(self) -> int:
        return self._events_written

    def observe(
        self,
        frame_index: int,
        masks: Sequence[InstanceMask] | Iterable[InstanceMask],
    ) -> None:
        """Update tracks from current-frame masks; emit enter/leave as needed."""
        if self._closed:
            raise CueLogError("cue log writer is closed")
        if isinstance(frame_index, bool) or not isinstance(frame_index, int) or frame_index < 0:
            raise CueLogError("frame_index must be a non-negative integer")
        if self._last_frame_index >= 0 and frame_index < self._last_frame_index:
            raise CueLogError("frame_index must be non-decreasing")
        self._last_frame_index = frame_index

        # Geometry constructs this fixed schema; retain that contract instead
        # of propagating an object-valued mapping through the tracker.
        observations = tuple(cast(_Observation, _observation_from_mask(mask)) for mask in masks)
        matched_active: set[int] = set()
        matched_obs: set[int] = set()

        # Greedy nearest-neighbour match within class and distance budget.
        candidates: list[tuple[float, int, int]] = []
        for active_index, track in enumerate(self._active):
            for obs_index, observation in enumerate(observations):
                if observation["class_id"] != track.class_id:
                    continue
                distance = math.hypot(
                    observation["centroid_x"] - track.centroid_x,
                    observation["centroid_y"] - track.centroid_y,
                )
                if distance <= self._match_distance_px:
                    candidates.append((distance, active_index, obs_index))
        candidates.sort(key=lambda item: (item[0], item[1], item[2]))

        for _distance, active_index, obs_index in candidates:
            if active_index in matched_active or obs_index in matched_obs:
                continue
            matched_active.add(active_index)
            matched_obs.add(obs_index)
            observation = observations[obs_index]
            track = self._active[active_index]
            track.centroid_x = observation["centroid_x"]
            track.centroid_y = observation["centroid_y"]
            track.confidence = observation["confidence"]
            track.class_name = observation["class_name"]
            track.last_frame_index = frame_index

        remaining_active = []
        for active_index, track in enumerate(self._active):
            if active_index in matched_active:
                remaining_active.append(track)
                continue
            self._emit(
                CueEvent(
                    schema_version=SCHEMA_VERSION,
                    event="leave",
                    frame_index=frame_index,
                    track_id=track.track_id,
                    class_id=track.class_id,
                    class_name=track.class_name,
                    confidence=track.confidence,
                    centroid_x=track.centroid_x,
                    centroid_y=track.centroid_y,
                )
            )
        self._active = remaining_active

        for obs_index, observation in enumerate(observations):
            if obs_index in matched_obs:
                continue
            track_id = self._next_track_id
            self._next_track_id += 1
            self._active.append(
                _ActiveTrack(
                    track_id=track_id,
                    class_id=observation["class_id"],
                    class_name=observation["class_name"],
                    confidence=observation["confidence"],
                    centroid_x=observation["centroid_x"],
                    centroid_y=observation["centroid_y"],
                    last_frame_index=frame_index,
                )
            )
            self._emit(
                CueEvent(
                    schema_version=SCHEMA_VERSION,
                    event="enter",
                    frame_index=frame_index,
                    track_id=track_id,
                    class_id=observation["class_id"],
                    class_name=observation["class_name"],
                    confidence=observation["confidence"],
                    centroid_x=observation["centroid_x"],
                    centroid_y=observation["centroid_y"],
                )
            )

    def close(self, *, commit: bool = True) -> None:
        """Finalize tracks and either commit the partial or discard it.

        When ``commit`` is true (default), remaining active tracks emit leave
        events, the partial is closed, and ``os.replace`` installs it as the
        destination. When false, the partial is removed and any pre-existing
        destination is left untouched.
        """
        if self._closed:
            if commit and not self._committed:
                raise CueLogError("cue log was already discarded; cannot commit after abort")
            return
        try:
            if commit:
                leave_frame = self._last_frame_index + 1 if self._last_frame_index >= 0 else 0
                for track in list(self._active):
                    self._emit(
                        CueEvent(
                            schema_version=SCHEMA_VERSION,
                            event="leave",
                            frame_index=leave_frame,
                            track_id=track.track_id,
                            class_id=track.class_id,
                            class_name=track.class_name,
                            confidence=track.confidence,
                            centroid_x=track.centroid_x,
                            centroid_y=track.centroid_y,
                        )
                    )
            self._active.clear()
            self._closed = True
            self._handle.close()
            if commit:
                os.replace(self._partial, self._path)
                self._committed = True
            else:
                self._partial.unlink(missing_ok=True)
        except Exception:
            self._closed = True
            if not self._handle.closed:
                self._handle.close()
            self._partial.unlink(missing_ok=True)
            raise

    def __enter__(self) -> CueLogWriter:
        return self

    def __exit__(self, exc_type: object, *_exc: object) -> None:
        self.close(commit=exc_type is None)

    def _emit(self, event: CueEvent) -> None:
        line = json.dumps(event.as_json(), sort_keys=True, separators=(",", ":"))
        self._handle.write(line + "\n")
        self._handle.flush()
        self._events_written += 1
