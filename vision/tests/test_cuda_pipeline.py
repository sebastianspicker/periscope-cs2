from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.inference.cuda.pipeline import (
    DetectionResult,
    GpuInferencePipeline,
    GpuPipelineConfig,
    _filter_by_confidence,
)


class GpuPipelineConfigTests(unittest.TestCase):
    def test_default_config_values(self) -> None:
        cfg = GpuPipelineConfig()
        self.assertEqual(cfg.image_size, 640)
        self.assertEqual(cfg.confidence, 0.4)
        self.assertEqual(cfg.iou_threshold, 0.5)
        self.assertEqual(cfg.num_classes, 4)
        self.assertTrue(cfg.use_fp16)
        self.assertTrue(cfg.enable_streaming)
        self.assertTrue(cfg.prealloc_buffers)


class DetectionResultTests(unittest.TestCase):
    def test_empty_detection(self) -> None:
        result = DetectionResult(
            boxes=np.empty((0, 4), dtype=np.float32),
            scores=np.empty(0, dtype=np.float32),
            class_ids=np.empty(0, dtype=np.int32),
            num_detections=0,
        )
        self.assertEqual(result.num_detections, 0)
        self.assertEqual(result.gpu_time_ms, 0.0)
        self.assertEqual(result.pipeline_time_ms, 0.0)


class GpuInferencePipelineCpuTests(unittest.TestCase):
    def setUp(self) -> None:
        self.session = MagicMock()
        self.session.run.return_value = [np.random.randn(1, 8, 100).astype(np.float32)]

    def test_init_with_cpu_fallback(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(self.session, num_classes=4)
            self.assertIsNotNone(pipeline)
            self.assertFalse(pipeline.cuda_available)
            self.assertIsNone(pipeline._io_binding)

    def test_run_returns_detection_result_on_cpu(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(self.session, num_classes=4)
            frame = np.zeros((480, 640, 3), dtype=np.uint8)
            result = pipeline.run(frame)
            self.assertIsInstance(result, DetectionResult)
            self.assertGreater(result.num_detections, 0)
            self.assertEqual(result.boxes.ndim, 2)
            self.assertEqual(result.boxes.shape[1], 4)
            self.assertEqual(result.scores.ndim, 1)
            self.assertEqual(result.class_ids.ndim, 1)

    def test_run_with_empty_frame_does_not_crash(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(self.session, num_classes=4)
            frame = np.zeros((1, 1, 3), dtype=np.uint8)
            result = pipeline.run(frame)
            self.assertIsInstance(result, DetectionResult)

    def test_avg_timing_properties(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(self.session, num_classes=4)
            self.assertEqual(pipeline.avg_gpu_ms, 0.0)
            self.assertEqual(pipeline.avg_pipeline_ms, 0.0)
            frame = np.zeros((480, 640, 3), dtype=np.uint8)
            pipeline.run(frame)
            self.assertGreater(pipeline.avg_pipeline_ms, 0)

    def test_session_run_called_with_correct_args(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(self.session, num_classes=4)
            frame = np.zeros((480, 640, 3), dtype=np.uint8)
            pipeline.run(frame)
            self.session.run.assert_called_once()
            args, _ = self.session.run.call_args
            self.assertEqual(args[0], ["output0"])


class GpuPipelineConfigCreationTests(unittest.TestCase):
    def test_config_creates_pipeline_with_overrides(self) -> None:
        cfg = GpuPipelineConfig(image_size=416, confidence=0.5, iou_threshold=0.6, num_classes=2)
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(
                self._make_session(),
                num_classes=cfg.num_classes,
                image_size=cfg.image_size,
                confidence=cfg.confidence,
                iou_threshold=cfg.iou_threshold,
            )
            self.assertEqual(pipeline.image_size, 416)
            self.assertEqual(pipeline.confidence, 0.5)
            self.assertEqual(pipeline.iou_threshold, 0.6)
            self.assertEqual(pipeline.num_classes, 2)

    def _make_session(self):
        session = MagicMock()
        session.run.return_value = [np.random.randn(1, 6, 100).astype(np.float32)]
        return session


class ConfidenceFilterTests(unittest.TestCase):
    """Confidence filter is part of the shipped GPU pipeline path."""

    def test_filter_drops_below_threshold(self) -> None:
        boxes = np.array([[0, 0, 10, 10], [10, 10, 20, 20], [20, 20, 30, 30]], dtype=np.float32)
        scores = np.array([0.9, 0.3, 0.5], dtype=np.float32)
        class_ids = np.array([0, 1, 0], dtype=np.int32)
        out_b, out_s, out_c = _filter_by_confidence(boxes, scores, class_ids, 0.4)
        self.assertEqual(len(out_s), 2)
        np.testing.assert_array_almost_equal(out_s, [0.9, 0.5])
        np.testing.assert_array_equal(out_c, [0, 0])

    def test_filter_empty_input(self) -> None:
        boxes = np.empty((0, 4), dtype=np.float32)
        scores = np.empty(0, dtype=np.float32)
        class_ids = np.empty(0, dtype=np.int32)
        out_b, out_s, out_c = _filter_by_confidence(boxes, scores, class_ids, 0.5)
        self.assertEqual(len(out_s), 0)
        self.assertEqual(len(out_b), 0)
        self.assertEqual(len(out_c), 0)

    def test_pipeline_run_applies_confidence(self) -> None:
        """Shipped run() must filter scores using the pipeline confidence."""
        # Craft a decode-friendly YOLO tensor: 1×(4+nc)×N with known scores.
        # decode_cpu uses argmax on class channels; set class0 high for box0,
        # class0 low for box1.
        num_classes = 2
        n = 4
        raw = np.zeros((1, 4 + num_classes, n), dtype=np.float32)
        # boxes as cx,cy,w,h in letterbox space (will be scaled)
        for i in range(n):
            raw[0, 0, i] = 320.0  # cx
            raw[0, 1, i] = 320.0  # cy
            raw[0, 2, i] = 64.0  # w
            raw[0, 3, i] = 64.0  # h
        # high conf for first two, low for last two
        raw[0, 4, 0] = 0.95  # class 0 score channel (argmax)
        raw[0, 5, 0] = 0.05
        raw[0, 4, 1] = 0.80
        raw[0, 5, 1] = 0.10
        raw[0, 4, 2] = 0.20
        raw[0, 5, 2] = 0.05
        raw[0, 4, 3] = 0.15
        raw[0, 5, 3] = 0.10

        session = MagicMock()
        session.run.return_value = [raw]
        with patch(
            "cs2_vision_access.inference.cuda.pipeline.cuda_available",
            return_value=False,
        ):
            pipeline = GpuInferencePipeline(
                session,
                num_classes=num_classes,
                confidence=0.5,
                iou_threshold=1.0,  # disable NMS suppression of near-identical boxes
            )
            frame = np.zeros((640, 640, 3), dtype=np.uint8)
            result = pipeline.run(frame)
            self.assertIsInstance(result, DetectionResult)
            self.assertFalse(result.on_gpu)
            # Only the two high-confidence detections should remain.
            self.assertEqual(result.num_detections, 2)
            self.assertTrue(all(float(s) >= 0.5 for s in result.scores))
