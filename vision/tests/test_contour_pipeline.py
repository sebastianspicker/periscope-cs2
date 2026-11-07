"""Tests for the contour-composite pipeline: geometry, sinks, pacing."""

from __future__ import annotations

import contextlib
import multiprocessing.shared_memory as shared_memory
import struct
import uuid
from unittest.mock import Mock

import numpy as np
import pytest

from cs2_vision_access.adapters.capture.live_pipeline.config import (
    LivePipelineConfig,
)
from cs2_vision_access.adapters.capture.live_pipeline.frame import run_frame
from cs2_vision_access.adapters.capture.presentation import (
    PRESENTATION_MODES,
    CompositeOptions,
    PresentGeometry,
    resolve_present_size,
    validate_present_mode,
)
from cs2_vision_access.adapters.capture.sinks.shared import (
    HEADER_FORMAT,
    HEADER_SIZE,
    MAGIC,
    STATE_IDLE,
    STATE_READY,
    SharedFrameSink,
    read_shared_frame,
    read_shared_header,
)
from cs2_vision_access.domain.outline import OutlineStyle


# ── Geometry ──────────────────────────────────────────────────────
def test_present_geometry_matches_1to1() -> None:
    geometry = resolve_present_size(1920, 1080, 1920, 1080)
    assert (geometry.width, geometry.height) == (1920, 1080)


def test_present_geometry_rejects_scaling() -> None:
    with pytest.raises(ValueError, match="does not match"):
        resolve_present_size(1920, 1080, 2560, 1440)


def test_present_geometry_rejects_zero() -> None:
    with pytest.raises(ValueError, match="positive"):
        resolve_present_size(0, 1080, 1920, 1080)
    with pytest.raises(ValueError, match="positive"):
        resolve_present_size(1920, 1080, 0, 1080)


def test_present_offsets_are_integers() -> None:
    geometry = PresentGeometry(width=1920, height=1080, offset_x=10, offset_y=-4)
    assert geometry.offset_x == 10
    assert geometry.offset_y == -4
    with pytest.raises(ValueError, match="integers"):
        PresentGeometry(width=1920, height=1080, offset_x=1.5)  # type: ignore[arg-type]


# ── Modes ─────────────────────────────────────────────────────────
def test_presentation_modes_accepted() -> None:
    for mode in PRESENTATION_MODES:
        assert validate_present_mode(mode) == mode


def test_presentation_mode_rejected() -> None:
    with pytest.raises(ValueError, match="unsupported presentation mode"):
        validate_present_mode("bgr")


def test_composite_options_defaults() -> None:
    options = CompositeOptions(outline=OutlineStyle())
    assert options.contour_only is False
    assert options.show_fill is False


# ── Shared ring ───────────────────────────────────────────────────
@pytest.fixture()
def ring() -> SharedFrameSink:
    name = f"ps-ring-{uuid.uuid4().hex[:12]}"
    sink = SharedFrameSink(name, 8, 4, slots=2, fps_hint=60)
    yield sink
    # Unlink explicitly so the segment disappears even on shared_memory
    # implementations that keep the object alive until the last mapping closes.
    with contextlib.suppress(FileNotFoundError):
        shared_memory.SharedMemory(name=name).unlink()


def test_sink_header_layout(ring: SharedFrameSink) -> None:
    header = read_shared_header(ring._shm)
    assert (header.width, header.height) == (8, 4)
    assert header.stride == 32
    assert header.channels == 4
    assert header.state == STATE_IDLE
    assert header.frame_index == 0


def test_sink_publish_and_read(ring: SharedFrameSink) -> None:
    frame = np.zeros((4, 8, 4), dtype=np.uint8)
    frame[..., 3] = 255
    frame[2, 5, 0] = 200
    ring.send(frame, frame_index=0)
    got = read_shared_frame(ring._shm, 8, 4, slots=2)
    assert got.shape == (4, 8, 4)
    assert got[2, 5, 0] == 200
    assert got[2, 5, 3] == 255


def test_sink_header_updates_on_publish(ring: SharedFrameSink) -> None:
    frame = np.full((4, 8, 4), 7, dtype=np.uint8)
    ring.send(frame, frame_index=3)
    header = read_shared_header(ring._shm)
    assert header.state == STATE_READY
    assert header.frame_index == 4  # 1-based publish sequence
    assert header.fps_hint == 60


def test_sink_rejects_wrong_shape(ring: SharedFrameSink) -> None:
    with pytest.raises(ValueError, match="shape"):
        ring.send(np.zeros((4, 8, 3), dtype=np.uint8), frame_index=0)


def test_sink_rejects_wrong_size(ring: SharedFrameSink) -> None:
    with pytest.raises(ValueError, match="does not match"):
        ring.send(np.zeros((2, 8, 4), dtype=np.uint8), frame_index=0)


def test_sink_rejects_bad_frame_size_at_open() -> None:
    with pytest.raises(ValueError, match="positive"):
        SharedFrameSink("periscope-bad-ring", 0, 4)


def test_ring_survives_producer_restart() -> None:
    name = f"ps-ring-{uuid.uuid4().hex[:12]}"
    try:
        # First producer publishes, then dies.
        first = SharedFrameSink(name, 8, 4, slots=2, fps_hint=60)
        frame = np.full((4, 8, 4), 9, dtype=np.uint8)
        first.send(frame, frame_index=0)
        first.close()

        # A second producer recreates the same-name ring from scratch.
        second = SharedFrameSink(name, 8, 4, slots=2, fps_hint=60)
        try:
            header = read_shared_header(second._shm)
            assert header.state == STATE_IDLE
            assert header.frame_index == 0
        finally:
            second.close()
    finally:
        with contextlib.suppress(FileNotFoundError):
            shared_memory.SharedMemory(name=name).unlink()


def test_ring_magic_mismatch() -> None:
    import contextlib

    name = f"ps-ring-{uuid.uuid4().hex[:12]}"
    sink = SharedFrameSink(name, 4, 4)
    try:
        struct.pack_into("<I", sink._shm.buf, 0, 0)
        with pytest.raises(ValueError, match="magic"):
            read_shared_header(sink._shm)
    finally:
        sink.close()
        with contextlib.suppress(FileNotFoundError):
            shared_memory.SharedMemory(name=name).unlink()


def test_ring_header_wire_contract() -> None:
    # The wire contract consumers rely on; guard it against accidental change.
    assert HEADER_SIZE == 48
    assert MAGIC == 0x4356_3153
    # The constant is the big-endian spelling of the name "CV1S", the wire
    # bytes themselves read "S1VC" (see the MAGIC comment in shared.py).
    assert struct.pack(">I", MAGIC) == b"CV1S"
    assert struct.pack("<I", MAGIC) == b"S1VC"
    assert struct.calcsize(HEADER_FORMAT) == HEADER_SIZE


# ── Pacing ────────────────────────────────────────────────────────
@pytest.mark.parametrize(
    ("max_age_ms", "elapsed", "dropped", "reason"),
    [(20.0, 0.03, 1, None), (20.0, 0.02, 0, "eof"), (20.0, 0.01, 0, "eof"), (0.0, 1.0, 0, "eof")],
)
def test_capture_age_gate_consumes_stale_without_inference(
    monkeypatch: pytest.MonkeyPatch,
    max_age_ms: float,
    elapsed: float,
    dropped: int,
    reason: str | None,
) -> None:
    """Exercise the age boundary and disabled control through the real frame path."""
    monkeypatch.setattr(
        "cs2_vision_access.adapters.capture.live_pipeline.frame.time.perf_counter",
        lambda: elapsed,
    )
    capture = Mock()
    capture.read.return_value = (False, None, "eof")
    segmenter = Mock()
    renderer = Mock()
    display = Mock()
    callback = Mock()
    result = run_frame(
        frame_index=0,
        capture_mgr=capture,
        segmenter=segmenter,
        cfg=LivePipelineConfig(max_capture_age_ms=max_age_ms, frame_skip_threshold=1000),
        capture_fps=100.0,
        started=0.0,
        base_outline_style=OutlineStyle(),
        renderer=renderer,
        temporal_policy=None,
        control_state=Mock(),
        display_mgr=display,
        on_frame=callback,
        diag=Mock(),
        last_temporal_enabled=False,
        last_output_mode="overlay",
        last_alpha_fill=False,
    )
    capture.read.assert_called_once_with(0)
    assert result.frames_dropped == dropped
    assert result.frames_inferred_delta == 0
    assert result.termination_reason == reason
    segmenter.predict.assert_not_called()
    renderer.render.assert_not_called()
    display.show_frame.assert_not_called()
    callback.assert_not_called()
