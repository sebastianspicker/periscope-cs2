"""Regression tests for the live pipeline core and model loading."""

from __future__ import annotations

import contextlib
import multiprocessing.shared_memory as shared_memory
import time
import uuid
from pathlib import Path
from typing import Any
from unittest.mock import Mock

import numpy as np
import pytest

from cs2_vision_access.adapters.capture.base import CaptureConfig
from cs2_vision_access.adapters.capture.live_pipeline.config import LivePipelineConfig
from cs2_vision_access.adapters.capture.live_pipeline.diagnostics import _Diagnostics, _percentile
from cs2_vision_access.adapters.capture.live_pipeline.frame import run_frame
from cs2_vision_access.adapters.capture.manager import CaptureInfo, CaptureManager
from cs2_vision_access.adapters.capture.sinks.shared import SharedFrameSink, read_shared_frame
from cs2_vision_access.adapters.models.segmenters import _detect_common, cs2_sam
from cs2_vision_access.adapters.models.segmenters.protocol import SegmenterError
from cs2_vision_access.application.live.inference.temporal import (
    SuppressOnlyTemporalPolicy,
    TemporalStabilityConfig,
)
from cs2_vision_access.application.model_assets.manifest import ModelManifest
from cs2_vision_access.domain.outline import OutlineStyle
from cs2_vision_access.domain.predictions import InstanceMask


# ── 1. Shared ring newest-frame read ──────────────────────────────
@pytest.mark.parametrize("slots", [2, 3])
def test_shared_ring_returns_newest_frame(slots: int) -> None:
    name = f"ps-ring-{uuid.uuid4().hex[:12]}"
    sink = SharedFrameSink(name, 8, 4, slots=slots)
    try:
        for index in range(5):
            sink.send(np.full((4, 8, 4), 10 + index, dtype=np.uint8), frame_index=index)
            got = read_shared_frame(sink._shm, 8, 4, slots=slots)
            assert int(got[0, 0, 0]) == 10 + index
    finally:
        with contextlib.suppress(FileNotFoundError):
            shared_memory.SharedMemory(name=name).unlink()


# ── 2/3. Capture manager ──────────────────────────────────────────
def _manager(source_type: str, reads: list[tuple[bool, Any]]) -> CaptureManager:
    manager = CaptureManager(CaptureConfig(source="x.mp4"), Mock(), max_consecutive_read_failures=3)
    manager._info = CaptureInfo(width=8, height=8, fps=30.0, source_type=source_type)
    manager._cap = Mock()
    manager._cap.read.side_effect = reads
    return manager


def test_file_read_failure_is_end_of_stream(monkeypatch: pytest.MonkeyPatch) -> None:
    sleep = Mock()
    monkeypatch.setattr("cs2_vision_access.adapters.capture.manager.time.sleep", sleep)
    manager = _manager("file", [(False, None)])
    assert manager.read(0) == (False, None, "end_of_stream")
    sleep.assert_not_called()


def test_successful_read_resets_failure_counter(monkeypatch: pytest.MonkeyPatch) -> None:
    sleep = Mock()
    monkeypatch.setattr("cs2_vision_access.adapters.capture.manager.time.sleep", sleep)
    good = (True, np.full((8, 8, 3), 100, dtype=np.uint8))
    bad = (False, None)
    manager = _manager("capture-device", [bad, bad, good, bad, bad, good, bad, bad])
    results = [manager.read(i) for i in range(8)]
    assert all(reason == "" for _ok, _frame, reason in results)
    sleep.assert_not_called()


# ── 4. Temporal history survives capture-side frame gaps ──────────
def _mask(frame_index: int) -> InstanceMask:
    polygon = ((10.0, 10.0), (50.0, 10.0), (50.0, 50.0), (10.0, 50.0))
    return InstanceMask(frame_index, polygon, 0.9, 0, "player")


def test_temporal_filter_survives_frame_gaps() -> None:
    policy = SuppressOnlyTemporalPolicy(
        TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
    )
    segmenter = Mock()
    segmenter.predict.side_effect = lambda _frame, frame_index: [_mask(frame_index)]
    capture = Mock()
    capture.read.return_value = (True, np.zeros((8, 8, 3), dtype=np.uint8), "")
    control = Mock()
    control.temporal_enabled = True
    control.output_mode = "overlay"
    control.alpha_fill = False
    shown: list[int] = []
    renderer = Mock()
    renderer.render.side_effect = lambda _f, preds, frame_index: shown.append(len(preds))
    inferred = 0
    for frame_index in (0, 1, 2, 5, 7, 10):  # capture-side gaps between inferred frames
        result = run_frame(
            frame_index=frame_index,
            capture_mgr=capture,
            segmenter=segmenter,
            cfg=LivePipelineConfig(headless=True, frame_skip_threshold=1000),
            capture_fps=1.0,
            started=time.perf_counter(),
            base_outline_style=OutlineStyle(),
            renderer=renderer,
            temporal_policy=policy,
            control_state=control,
            display_mgr=Mock(),
            on_frame=None,
            diag=_Diagnostics(),
            last_temporal_enabled=True,
            last_output_mode="overlay",
            last_alpha_fill=False,
            inferred_seq=inferred,
        )
        inferred += result.frames_inferred_delta
    assert shown == [0, 1, 1, 1, 1, 1]


# ── 5. Bounded diagnostics ────────────────────────────────────────
def test_diagnostics_are_bounded() -> None:
    diag = _Diagnostics()
    for value in range(20_000):
        diag.inference_ms.append(float(value))
        diag.pipeline_ms.append(float(value))
    assert len(diag.inference_ms) == 10_000
    assert len(diag.pipeline_ms) == 10_000
    assert _percentile(list(diag.inference_ms), 50) == pytest.approx(14_999.5)
    diag.maybe_print(10.0, 0.0, 5, 0.0, 0, 0, 0)  # exercises the last-100 average


# ── 6. Checksum bypass ────────────────────────────────────────────
def _load(model: Path, manifest: Path | None) -> ModelManifest:
    return _detect_common.load_manifest_or_default(
        model, manifest, model_filename="m.onnx", origin="o", license_name="l"
    )


def test_missing_manifest_path_raises(tmp_path: Path) -> None:
    with pytest.raises(SegmenterError, match="register-model"):
        _load(tmp_path / "m.onnx", tmp_path / "missing.json")


def test_no_manifest_path_uses_default(tmp_path: Path) -> None:
    assert len(_load(tmp_path / "m.onnx", None).classes) == 80


def test_sam_digest_mismatch_raises_before_ort_session(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    manifest = ModelManifest(1, "m.onnx", "", "detect", {0: "player"}, "o", "l")
    monkeypatch.setattr(cs2_sam, "verify_model", lambda *_a: (None, manifest))

    def fail(*_a: object, **_k: object) -> None:
        raise AssertionError("ORT session created before SHA-256 verification")

    monkeypatch.setattr("cs2_vision_access.adapters.models.create_ort_session", fail)
    monkeypatch.delenv("CS2_SAM_ENCODER", raising=False)
    monkeypatch.delenv("CS2_SAM_DECODER", raising=False)
    # Tampered files at the default (pinned) locations next to the detector.
    (tmp_path / cs2_sam._DEFAULT_ENCODER).write_bytes(b"tampered")
    (tmp_path / cs2_sam._DEFAULT_DECODER).write_bytes(b"tampered")
    with pytest.raises(SegmenterError, match="SHA-256"):
        cs2_sam.Cs2SamSegmenter(
            tmp_path / "m.onnx",
            tmp_path / "m.json",
            class_names=None,
            confidence=0.5,
            image_size=640,
            device="cpu",
        )
    with pytest.raises(SegmenterError, match="not found"):
        cs2_sam.Cs2SamSegmenter(
            tmp_path / "m.onnx",
            tmp_path / "m.json",
            class_names=None,
            confidence=0.5,
            image_size=640,
            device="cpu",
            sam_encoder_path=tmp_path / "missing.onnx",
        )


@pytest.mark.parametrize("is_file", [False, True])
def test_only_existing_files_are_classified_as_file(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, is_file: bool
) -> None:
    source = tmp_path / "clip.mp4"
    if is_file:
        source.write_bytes(b"x")
    else:
        source = Path("USB Video Device")  # named capture device, not a path on disk
    cap = Mock()
    cap.isOpened.return_value = True
    cap.get.side_effect = [64.0, 48.0, 30.0]  # width, height, fps
    monkeypatch.setattr(
        "cs2_vision_access.adapters.capture.manager.open_capture", lambda _config: cap
    )
    manager = CaptureManager(CaptureConfig(source=str(source)), Mock())
    manager.open()
    assert manager.info.source_type == ("file" if is_file else "capture-device")
