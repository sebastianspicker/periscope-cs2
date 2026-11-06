"""Sanitized session recording and replay (ledger V2).

Adapted from the research ledger: tiansongyu's snapshot-recorder doctrine —
opt-in, bounded, sanitized NDJSON with absolute-state records and a capture
timestamp per record — applied to the Vision track's prediction stream.

Design rules carried over:

- **Absolute records.** Every frame record is complete for its frame index;
  replay never needs reconstruction, seek is trivial, and a dropped frame
  leaves a gap rather than a corruption (the A4 incident class cannot
  corrupt a recording).
- **Sanitized at write.** The recorder is the single choke point: no process
  addresses exist in this data model, class names are pattern-validated, and
  non-finite values never reach the file.
- **Bounded.** A recording stops at ``max_records`` instead of growing until
  the disk is full (the A4 lesson applied to recordings).

Only stdlib is used; the caller injects the text sink so tests run against
``io.StringIO`` and production against an opened file.
"""

from __future__ import annotations

import json
import math
import re
import time
from collections.abc import Callable, Iterable
from dataclasses import dataclass
from typing import TextIO

from cs2_vision_access.domain.predictions import InstanceMask, Point

RECORDING_SCHEMA_VERSION = 1
_SAFE_NAME = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
_DEFAULT_MAX_RECORDS = 100_000  # ≈ 28 min at 60 fps; bounded by design


class RecordingError(ValueError):
    """A recording or replay violated the v1 contract."""


def _finite(value: float) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def sanitize_polygon(polygon: Iterable[Point], dropped: list[str]) -> tuple[Point, ...] | None:
    """Returns a sanitized polygon, or None when the instance must be dropped.

    Non-finite coordinates drop the *instance* (a half-sanitized contour is
    worse than no contour); the reason is appended to ``dropped`` so the
    caller can count losses.
    """
    points: list[Point] = []
    for x, y in polygon:
        if not _finite(x) or not _finite(y):
            dropped.append("non-finite coordinate")
            return None
        points.append((round(float(x), 3), round(float(y), 3)))
    if len(points) < 3:
        dropped.append("degenerate polygon")
        return None
    return tuple(points)


def sanitize_class_name(name: str, dropped: list[str]) -> str | None:
    if not _SAFE_NAME.fullmatch(name):
        dropped.append(f"unsafe class name: {name!r}")
        return None
    return name


def sanitize_source_label(label: str) -> str:
    """Header metadata: strip everything that is not safe to share.

    Quotes, backslashes, slashes and control characters become spaces,
    whitespace runs collapse, and leading ``../`` traversal sequences are
    removed — the label is metadata in a shareable file, not a path.
    """
    cleaned = "".join(char if char.isprintable() and char not in '"\\/' else " " for char in label)
    cleaned = re.sub(r"\s+", " ", cleaned).strip()
    cleaned = re.sub(r"^(?:\.\. )+", "", cleaned)
    return cleaned[:120] or "unknown"


@dataclass(frozen=True)
class FrameRecord:
    """One absolute frame of predictions, ready for JSON."""

    frame_index: int
    timestamp_ns: int
    instances: tuple[InstanceMask, ...]

    def to_json(self) -> str:
        instances = [
            {
                "c": instance.class_id,
                "name": instance.class_name,
                "conf": round(instance.confidence, 4),
                "poly": [[x, y] for x, y in instance.polygon],
            }
            for instance in self.instances
        ]
        return (
            json.dumps(
                {
                    "v": RECORDING_SCHEMA_VERSION,
                    "type": "frame",
                    "i": self.frame_index,
                    "t_ns": self.timestamp_ns,
                    "n": len(instances),
                    "instances": instances,
                },
                separators=(",", ":"),
                sort_keys=True,
            )
            + "\n"
        )


def _sanitize_instances(
    instances: Iterable[InstanceMask], dropped: list[str], frame_index: int
) -> tuple[InstanceMask, ...]:
    sanitized: list[InstanceMask] = []
    for instance in instances:
        polygon = sanitize_polygon(instance.polygon, dropped)
        name = sanitize_class_name(instance.class_name, dropped)
        if polygon is None or name is None:
            continue
        if not _finite(instance.confidence):
            dropped.append("non-finite confidence")
            continue
        confidence = min(max(float(instance.confidence), 0.0), 1.0)
        sanitized.append(
            InstanceMask(
                frame_index=frame_index,
                polygon=polygon,
                confidence=confidence,
                class_id=instance.class_id,
                class_name=name,
            )
        )
    return tuple(sanitized)


class SessionRecorder:
    """Writes a sanitized NDJSON recording; opt-in, bounded, absolute."""

    def __init__(
        self,
        sink: TextIO,
        *,
        width: int,
        height: int,
        source_label: str = "unknown",
        max_records: int = _DEFAULT_MAX_RECORDS,
        clock: Callable[[], int] | None = None,
    ) -> None:
        if width <= 0 or height <= 0:
            raise RecordingError("recording frame size must be positive")
        if max_records < 1:
            raise RecordingError("max_records must be positive")
        self._sink = sink
        self._max_records = max_records
        self._records = 0
        self._stopped = False
        self.dropped_reasons: list[str] = []
        self._clock = clock or time.time_ns
        header = {
            "v": RECORDING_SCHEMA_VERSION,
            "type": "header",
            "width": int(width),
            "height": int(height),
            "source": sanitize_source_label(source_label),
            "created_ns": self._clock(),
        }
        self._sink.write(json.dumps(header, separators=(",", ":"), sort_keys=True) + "\n")
        self._sink.flush()

    @property
    def stopped(self) -> bool:
        return self._stopped

    @property
    def records(self) -> int:
        return self._records

    def record_indexed(
        self,
        frame_index: int,
        instances: Iterable[InstanceMask],
        timestamp_ns: int | None = None,
    ) -> bool:
        """Canonical entry point: one absolute record per pipeline frame.

        Returns False when recording has stopped (cap reached) — callers keep
        running, the file just stops growing.
        """
        if self._stopped:
            return False
        if frame_index < 0:
            raise RecordingError("frame_index must be non-negative")
        stamp = self._clock() if timestamp_ns is None else int(timestamp_ns)
        if stamp < 0:
            raise RecordingError("timestamp_ns must be non-negative")
        sanitized = _sanitize_instances(instances, self.dropped_reasons, frame_index)
        return self._write_frame(frame_index, stamp, sanitized)

    def _write_frame(
        self, frame_index: int, timestamp_ns: int, instances: tuple[InstanceMask, ...]
    ) -> bool:
        if self._records >= self._max_records:
            self._stopped = True
            return False
        record = FrameRecord(
            frame_index=frame_index,
            timestamp_ns=timestamp_ns,
            instances=instances,
        )
        self._sink.write(record.to_json())
        self._sink.flush()
        self._records += 1
        if self._records >= self._max_records:
            self._stopped = True
        return True


@dataclass(frozen=True)
class ReplayFrame:
    """One decoded frame from a recording."""

    frame_index: int
    timestamp_ns: int
    instances: tuple[InstanceMask, ...]


def _parse_frame(payload: dict[str, object]) -> ReplayFrame:
    frame_index = payload.get("i")
    timestamp_ns = payload.get("t_ns")
    raw_instances = payload.get("instances")
    if not isinstance(frame_index, int) or frame_index < 0:
        raise RecordingError("frame record has no valid index")
    if not isinstance(timestamp_ns, int) or timestamp_ns < 0:
        raise RecordingError("frame record has no valid timestamp")
    if not isinstance(raw_instances, list):
        raise RecordingError("frame record has no instance list")
    instances: list[InstanceMask] = []
    for raw in raw_instances:
        if not isinstance(raw, dict):
            raise RecordingError("instance must be an object")
        name = raw.get("name")
        polygon_raw = raw.get("poly")
        confidence = raw.get("conf")
        class_id = raw.get("c")
        if (
            not isinstance(name, str)
            or not _SAFE_NAME.fullmatch(name)
            or not isinstance(polygon_raw, list)
            or not isinstance(confidence, (int, float))
            or not math.isfinite(float(confidence))
            or not isinstance(class_id, int)
            or class_id < 0
        ):
            raise RecordingError("malformed instance in recording")
        polygon: list[Point] = []
        for point in polygon_raw:
            if (
                not isinstance(point, (list, tuple))
                or len(point) != 2
                or not all(_finite(coordinate) for coordinate in point)
            ):
                raise RecordingError("malformed polygon in recording")
            polygon.append((float(point[0]), float(point[1])))
        if len(polygon) < 3:
            raise RecordingError("polygon with fewer than three points")
        instances.append(
            InstanceMask(
                frame_index=frame_index,
                polygon=tuple(polygon),
                confidence=float(confidence),
                class_id=class_id,
                class_name=name,
            )
        )
    return ReplayFrame(frame_index, timestamp_ns, tuple(instances))


class SessionReplay:
    """Reads a recording. Unknown record types are skipped (forward compat);
    malformed frame records raise RecordingError — a corrupt file must never
    look like an empty session."""

    def __init__(self, source: TextIO) -> None:
        self._source = source
        self._header: dict[str, object] | None = None

    @property
    def header(self) -> dict[str, object]:
        if self._header is None:
            raise RecordingError("replay not started (header not read)")
        return self._header

    def read_header(self) -> dict[str, object]:
        for line in self._source:
            stripped = line.strip()
            if not stripped:
                continue
            payload = json.loads(stripped)
            if payload.get("type") != "header":
                raise RecordingError("recording does not start with a header")
            if payload.get("v") != RECORDING_SCHEMA_VERSION:
                raise RecordingError(f"unsupported recording schema {payload.get('v')}")
            self._header = payload
            return self._header
        raise RecordingError("empty recording")

    def frames(self) -> Iterable[ReplayFrame]:
        if self._header is None:
            self.read_header()
        for line in self._source:
            stripped = line.strip()
            if not stripped:
                continue
            payload = json.loads(stripped)
            kind = payload.get("type")
            if kind == "frame":
                yield _parse_frame(payload)
            # Unknown types are skipped silently (forward compat).

    def seek(self, frame_index: int) -> ReplayFrame | None:
        """First recorded frame with index >= frame_index (seek is trivial
        because records are absolute). None when the recording ends first."""
        for frame in self.frames():
            if frame.frame_index >= frame_index:
                return frame
        return None


def sanitize_for_tooling(value: object) -> object:
    """Recursive sanity pass for tooling that re-serializes records:
    non-finite floats become 0 so a JSON mirror can never emit NaN."""
    if isinstance(value, float):
        return value if math.isfinite(value) else 0.0
    if isinstance(value, dict):
        return {key: sanitize_for_tooling(item) for key, item in value.items()}
    if isinstance(value, list):
        return [sanitize_for_tooling(item) for item in value]
    return value
