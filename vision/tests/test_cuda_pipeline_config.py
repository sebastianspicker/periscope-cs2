"""Tests for the GpuPipelineConfig parameter and CPU fallback in GpuInferencePipeline."""

from __future__ import annotations

import contextlib
import unittest
from types import SimpleNamespace
from typing import Any
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.inference.cuda.memory import GpuMemoryPool
from cs2_vision_access.inference.cuda.pipeline import (
    DetectionResult,
    GpuInferencePipeline,
    GpuPipelineConfig,
    _session_input_dtype,
)


@contextlib.contextmanager
def _cuda_disabled():
    with (
        patch("cs2_vision_access.inference.cuda.utils.cuda_available", return_value=False),
        patch("cs2_vision_access.inference.cuda.preprocess.cuda_available", return_value=False),
        patch("cs2_vision_access.inference.cuda.pipeline.cuda_available", return_value=False),
        patch("cs2_vision_access.inference.cuda.decode.cuda_available", return_value=False),
        patch("cs2_vision_access.inference.cuda.nms.cuda_available", return_value=False),
        patch("cs2_vision_access.inference.cuda.memory.cuda_available", return_value=False),
    ):
        yield


def _fake_session() -> MagicMock:
    return MagicMock()


def _session_with_detection() -> MagicMock:
    raw = np.zeros((1, 6, 3), dtype=np.float32)
    raw[0, 0, 0] = 160.0  # cx in det_size space
    raw[0, 1, 0] = 160.0  # cy
    raw[0, 2, 0] = 64.0  # w
    raw[0, 3, 0] = 64.0  # h
    raw[0, 4, 0] = 0.9  # class 0 score
    raw[0, 5, 0] = 0.05
    raw[0, 4, 1] = 0.2
    raw[0, 5, 1] = 0.1
    raw[0, 4, 2] = 0.1
    raw[0, 5, 2] = 0.05
    session = MagicMock()
    session.run.return_value = [raw]
    return session


class GpuPipelineConfigOverrideTests(unittest.TestCase):
    def test_config_defaults_applied(self) -> None:
        cfg = GpuPipelineConfig(num_classes=2, image_size=320, confidence=0.3, iou_threshold=0.4)
        with _cuda_disabled():
            pipeline = GpuInferencePipeline(_fake_session(), config=cfg)
        self.assertEqual(pipeline.num_classes, 2)
        self.assertEqual(pipeline.image_size, 320)
        self.assertEqual(pipeline.confidence, 0.3)
        self.assertEqual(pipeline.iou_threshold, 0.4)

    def test_explicit_kwargs_win_over_config(self) -> None:
        cfg = GpuPipelineConfig(num_classes=2, image_size=320)
        with _cuda_disabled():
            pipeline = GpuInferencePipeline(_fake_session(), config=cfg, num_classes=5)
        self.assertEqual(pipeline.num_classes, 5)
        self.assertEqual(pipeline.image_size, 320)

    def test_no_config_uses_class_defaults(self) -> None:
        with _cuda_disabled():
            pipeline = GpuInferencePipeline(_fake_session(), num_classes=4, image_size=640)
        self.assertEqual(pipeline.num_classes, 4)
        self.assertEqual(pipeline.image_size, 640)
        self.assertEqual(pipeline.confidence, 0.4)
        self.assertEqual(pipeline.iou_threshold, 0.5)


class SessionInputDtypeTests(unittest.TestCase):
    def test_float16(self) -> None:
        session = MagicMock()
        session.get_inputs.return_value = [SimpleNamespace(type="tensor(float16)")]
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float16))

    def test_float32(self) -> None:
        session = MagicMock()
        session.get_inputs.return_value = [SimpleNamespace(type="tensor(float)")]
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float32))

    def test_float64(self) -> None:
        session = MagicMock()
        session.get_inputs.return_value = [SimpleNamespace(type="tensor(double)")]
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float64))

    def test_missing_type_falls_back_to_float32(self) -> None:
        session = MagicMock()
        session.get_inputs.return_value = [object()]
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float32))

    def test_empty_inputs_falls_back_to_float32(self) -> None:
        session = MagicMock()
        session.get_inputs.return_value = []
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float32))

    def test_get_inputs_raises_falls_back_to_float32(self) -> None:
        session = MagicMock()
        session.get_inputs.side_effect = RuntimeError("no session")
        self.assertEqual(_session_input_dtype(session), np.dtype(np.float32))


class GpuPipelineCpuRunWithConfigTests(unittest.TestCase):
    def test_run_returns_detection_result_with_config(self) -> None:
        cfg = GpuPipelineConfig(num_classes=2, image_size=320, confidence=0.3, iou_threshold=0.4)
        with _cuda_disabled():
            pipeline = GpuInferencePipeline(_session_with_detection(), config=cfg)
            frame = np.zeros((320, 320, 3), dtype=np.uint8)
            result = pipeline.run(frame)
        self.assertIsInstance(result, DetectionResult)
        self.assertGreaterEqual(result.num_detections, 1)
        self.assertEqual(result.boxes.ndim, 2)
        self.assertEqual(result.boxes.shape[1], 4)
        self.assertEqual(result.scores.ndim, 1)
        self.assertEqual(result.class_ids.ndim, 1)
        self.assertFalse(result.on_gpu)


class GpuPipelineCpuStreamFallbackTests(unittest.TestCase):
    def test_run_succeeds_when_streams_not_enabled(self) -> None:
        with _cuda_disabled():
            pipeline = GpuInferencePipeline(
                _session_with_detection(),
                config=GpuPipelineConfig(num_classes=2, image_size=320),
            )
            self.assertFalse(pipeline._streams_enabled())
            frame = np.zeros((320, 320, 3), dtype=np.uint8)
            result = pipeline.run(frame)
        self.assertIsInstance(result, DetectionResult)
        self.assertGreaterEqual(result.num_detections, 1)
        self.assertFalse(result.on_gpu)
        # The stream code path must never run while CUDA is unavailable.
        self.assertFalse(pipeline._streams_enabled())


class GpuMemoryPoolSliceTests(unittest.TestCase):
    """Key-selection in ``GpuMemoryPool.slice_for`` without a real GPU."""

    def _make_pool(self, buffers: dict[str, Any]) -> GpuMemoryPool:
        pool = object.__new__(GpuMemoryPool)
        pool._buffers = buffers
        return pool

    def test_slice_for_boxes_returns_3x4_view_of_8400x4(self) -> None:
        boxes = np.zeros((8400, 4), dtype=np.float32)
        pool = self._make_pool({"boxes_8400x4": boxes})
        view = pool.slice_for("boxes", 3, 4)
        self.assertIsNotNone(view)
        self.assertEqual(view.shape, (3, 4))
        self.assertTrue(np.shares_memory(view, boxes))

    def test_slice_for_scores_returns_5_view_of_8400(self) -> None:
        scores = np.zeros((8400,), dtype=np.float32)
        pool = self._make_pool({"scores_8400": scores})
        view = pool.slice_for("scores", 5)
        self.assertIsNotNone(view)
        self.assertEqual(view.shape, (5,))
        self.assertTrue(np.shares_memory(view, scores))

    def test_slice_for_returns_none_when_no_8400_buffer_matches(self) -> None:
        pool = self._make_pool({"det_output_1x88x8400": np.zeros((1, 88, 8400))})
        self.assertIsNone(pool.slice_for("nms_suppressed", 3))

    def test_prealloc_class_ids_is_int32(self) -> None:
        """``class_ids`` must be an integer dtype so GPU decode can use it as
        an index into the class-probability block (float32 would raise on
        CuPy and silently downgrade GPU decode to CPU)."""
        created: dict[str, Any] = {}

        class _FakeCp:
            int32 = np.int32
            float32 = np.float32

            @staticmethod
            def empty(shape: tuple[int, ...], dtype: Any) -> Any:
                created["dtype"] = dtype
                return np.zeros(shape, dtype=dtype)

        pool = object.__new__(GpuMemoryPool)
        pool._cp = _FakeCp()  # type: ignore[attr-defined]
        pool._buffers = {}
        pool._prealloc("class_ids", 8400, dtype=_FakeCp.int32)
        self.assertEqual(created["dtype"], np.int32)
        self.assertEqual(pool._buffers["class_ids_8400"].dtype, np.int32)


if __name__ == "__main__":
    unittest.main()
