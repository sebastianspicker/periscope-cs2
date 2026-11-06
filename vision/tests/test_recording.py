"""Ledger V2 tests: sanitized recording + replay (acceptance criteria).

- record → replay byte-stability (round-trip through the exact file bytes)
- seek determinism (absolute records; same seek → same frame, every time)
- sanitizer: non-finite coordinates/confidence, degenerate polygons, unsafe
  class names, and hostile source labels never reach the file
- bounded recording stops instead of growing forever (A4 lesson)
- malformed recordings raise; unknown record types are skipped
"""

from __future__ import annotations

import io
import json
import math
from typing import Any

import pytest

from cs2_vision_access.application.live.recording import (
    RecordingError,
    SessionRecorder,
    SessionReplay,
    sanitize_for_tooling,
    sanitize_source_label,
)
from cs2_vision_access.domain.predictions import InstanceMask, Point


def _mask(frame_index: int = 0, name: str = "player", confidence: float = 0.9) -> InstanceMask:
    polygon: tuple[Point, ...] = ((10.0, 10.0), (50.0, 10.0), (50.0, 50.0))
    return InstanceMask(frame_index, polygon, confidence, 0, name)


def _recorder(sink: io.StringIO, **kwargs: Any) -> SessionRecorder:
    defaults: dict[str, Any] = {"width": 1920, "height": 1080, "source_label": "lab capture"}
    defaults.update(kwargs)
    return SessionRecorder(sink, **defaults)


def test_record_replay_round_trip_byte_stability() -> None:
    sink = io.StringIO()
    recorder = _recorder(sink)
    for index in range(5):
        recorder.record_indexed(index, [_mask(frame_index=index)], timestamp_ns=1_000 + index)

    recorded = sink.getvalue()
    replay = SessionReplay(io.StringIO(recorded))
    header = replay.read_header()
    assert header["width"] == 1920
    frames = list(replay.frames())
    assert [frame.frame_index for frame in frames] == [0, 1, 2, 3, 4]
    assert [frame.timestamp_ns for frame in frames] == [1_000, 1_001, 1_002, 1_003, 1_004]
    for frame in frames:
        assert len(frame.instances) == 1
        assert frame.instances[0].class_name == "player"
        assert frame.instances[0].polygon == ((10.0, 10.0), (50.0, 10.0), (50.0, 50.0))

    # Byte stability: re-recording the same decoded instances yields the
    # same frame lines (header differs only by created_ns/source).
    second = io.StringIO()
    re_recorder = _recorder(second)
    for frame in frames:
        re_recorder.record_indexed(
            frame.frame_index, frame.instances, timestamp_ns=frame.timestamp_ns
        )
    original_frames = [line for line in recorded.splitlines() if '"type":"frame"' in line]
    rewritten_frames = [line for line in second.getvalue().splitlines() if '"type":"frame"' in line]
    assert original_frames == rewritten_frames


def test_seek_is_deterministic() -> None:
    sink = io.StringIO()
    recorder = _recorder(sink)
    for index in range(20):
        recorder.record_indexed(index * 2, [], timestamp_ns=index)
    for _ in range(3):
        replay = SessionReplay(io.StringIO(sink.getvalue()))
        replay.read_header()
        found = replay.seek(11)
        assert found is not None and found.frame_index == 12
        none = replay.seek(999)
        assert none is None


def test_sanitizer_drops_unsafe_instances_and_counts_reasons() -> None:
    sink = io.StringIO()
    recorder = _recorder(sink)
    nan = float("nan")
    inf = float("inf")

    class _StubInstance:  # duck-typed InstanceMask with an unsafe confidence
        frame_index = 0
        polygon = ((1.0, 1.0), (2.0, 2.0), (3.0, 3.0))
        confidence = nan
        class_id = 0
        class_name = "p"

    from cs2_vision_access.application.live.recording import sanitize_polygon

    degenerate_dropped: list[str] = []
    assert sanitize_polygon(((0.0, 0.0),), degenerate_dropped) is None
    assert degenerate_dropped == ["degenerate polygon"]
    recorder.record_indexed(
        0,
        [
            _mask(),
            InstanceMask(0, ((nan, 0.0), (1.0, 1.0), (2.0, 2.0)), 0.9, 0, "p"),
            InstanceMask(0, ((inf, 0.0), (1.0, 1.0), (2.0, 2.0)), 0.9, 0, "p"),
            InstanceMask(0, ((1.0, 1.0), (2.0, 2.0), (3.0, 3.0)), 0.9, 0, "bad name!"),
            _StubInstance(),  # type: ignore[arg-type]
        ],
        timestamp_ns=1,
    )
    assert recorder.dropped_reasons, "drops must be counted"
    # Exactly one instance survived.
    replay = SessionReplay(io.StringIO(sink.getvalue()))
    replay.read_header()
    frames = list(replay.frames())
    assert len(frames) == 1 and len(frames[0].instances) == 1
    raw = sink.getvalue()
    assert "nan" not in raw and "inf" not in raw, "non-finite never serialized"
    assert "bad name" not in raw, "unsafe class name never serialized"


def test_source_label_is_sanitized() -> None:
    assert sanitize_source_label('desktop "dev\\C:\\Users\\é\n') == ("desktop dev C: Users é")
    sink = io.StringIO()
    _recorder(sink, source_label="../../etc/passwd\x00")
    header_line = sink.getvalue().splitlines()[0]
    assert ".." not in header_line and "\x00" not in header_line


def test_recording_is_bounded() -> None:
    sink = io.StringIO()
    recorder = _recorder(sink, max_records=3)
    results = [recorder.record_indexed(i, [_mask()], timestamp_ns=i) for i in range(10)]
    assert results == [True, True, True, False, False, False, False, False, False, False]
    assert recorder.stopped
    assert recorder.records == 3
    replay = SessionReplay(io.StringIO(sink.getvalue()))
    replay.read_header()
    assert len(list(replay.frames())) == 3


def test_malformed_recording_raises_not_masquerades() -> None:
    good = io.StringIO()
    recorder = _recorder(good)
    recorder.record_indexed(0, [_mask()], timestamp_ns=1)
    lines = good.getvalue().splitlines()

    # A frame record with a broken instance must raise on read.
    broken = json.loads(lines[1])
    broken["instances"][0]["conf"] = "high"  # type: ignore[invalid-assignment]
    corrupted = io.StringIO(lines[0] + "\n" + json.dumps(broken) + "\n")
    replay = SessionReplay(corrupted)
    replay.read_header()
    with pytest.raises(RecordingError):
        list(replay.frames())

    # Unknown record types are skipped (forward compat).
    future = io.StringIO(
        lines[0]
        + "\n"
        + json.dumps({"v": 1, "type": "annotation", "data": "x"})
        + "\n"
        + lines[1]
        + "\n"
    )
    replay = SessionReplay(future)
    replay.read_header()
    frames = list(replay.frames())
    assert len(frames) == 1


def test_empty_and_headerless_recordings_raise() -> None:
    with pytest.raises(RecordingError):
        SessionReplay(io.StringIO("")).read_header()
    with pytest.raises(RecordingError):
        SessionReplay(io.StringIO('{"v":1,"type":"frame","i":0}\n')).read_header()
    with pytest.raises(RecordingError):
        SessionReplay(io.StringIO('{"v":9,"type":"header"}\n')).read_header()


def test_tooling_mirror_never_emits_nan() -> None:
    dirty: dict[str, Any] = {"a": float("nan"), "b": [1.0, float("inf"), "x"]}
    clean = sanitize_for_tooling(dirty)
    assert math.isfinite(clean["a"]) and clean["a"] == 0  # type: ignore[index]
    assert clean["b"][1] == 0  # type: ignore[index]
    assert json.dumps(clean) == '{"a": 0.0, "b": [1.0, 0.0, "x"]}'
