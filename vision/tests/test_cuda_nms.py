from __future__ import annotations

import unittest
from unittest.mock import patch

import numpy as np

from cs2_vision_access.inference.cuda.nms import _nms_cpu, nms


class CudaNmsCpuTests(unittest.TestCase):
    def test_empty_boxes(self) -> None:
        result = _nms_cpu(np.empty((0, 4), dtype=np.float32), np.empty(0, dtype=np.float32))
        self.assertEqual(result, [])

    def test_single_box_always_kept(self) -> None:
        boxes = np.array([[10, 10, 100, 100]], dtype=np.float32)
        scores = np.array([0.9], dtype=np.float32)
        result = _nms_cpu(boxes, scores)
        self.assertEqual(result, [0])

    def test_non_overlapping_boxes_all_kept(self) -> None:
        boxes = np.array(
            [[10, 10, 50, 50], [100, 100, 150, 150], [200, 200, 250, 250]],
            dtype=np.float32,
        )
        scores = np.array([0.9, 0.8, 0.7], dtype=np.float32)
        result = _nms_cpu(boxes, scores)
        self.assertEqual(len(result), 3)

    def test_overlapping_lower_score_suppressed(self) -> None:
        boxes = np.array(
            [[10, 10, 100, 100], [15, 15, 95, 95], [200, 200, 250, 250]],
            dtype=np.float32,
        )
        scores = np.array([0.9, 0.5, 0.7], dtype=np.float32)
        result = _nms_cpu(boxes, scores, iou_threshold=0.5)
        self.assertIn(0, result)
        self.assertIn(2, result)
        self.assertNotIn(1, result)

    def test_results_sorted_by_descending_score(self) -> None:
        boxes = np.array(
            [[10, 10, 100, 100], [200, 200, 300, 300], [15, 15, 95, 95]],
            dtype=np.float32,
        )
        scores = np.array([0.7, 0.9, 0.8], dtype=np.float32)
        result = _nms_cpu(boxes, scores, iou_threshold=0.5)
        self.assertEqual(result, [1, 2])

    def test_low_iou_threshold_keeps_all_non_overlapping(self) -> None:
        boxes = np.array(
            [[10, 10, 50, 50], [30, 30, 70, 70], [100, 100, 140, 140]],
            dtype=np.float32,
        )
        scores = np.array([0.9, 0.8, 0.7], dtype=np.float32)
        result = _nms_cpu(boxes, scores, iou_threshold=0.1)
        self.assertLess(len(result), 3)

    def test_high_iou_threshold_keeps_overlapping(self) -> None:
        boxes = np.array(
            [[10, 10, 100, 100], [15, 15, 95, 95]],
            dtype=np.float32,
        )
        scores = np.array([0.9, 0.8], dtype=np.float32)
        result = _nms_cpu(boxes, scores, iou_threshold=0.95)
        self.assertEqual(len(result), 2)


class CudaNmsPublicApiTests(unittest.TestCase):
    def test_public_nms_cpu_fallback_when_cuda_unavailable(self) -> None:
        with patch("cs2_vision_access.inference.cuda.nms.cuda_available", return_value=False):
            boxes = np.array([[10, 10, 100, 100]], dtype=np.float32)
            scores = np.array([0.9], dtype=np.float32)
            result = nms(boxes, scores)
            self.assertEqual(result, [0])

    def test_public_nms_fallback_on_gpu_exception(self) -> None:
        with patch("cs2_vision_access.inference.cuda.nms.cuda_available", return_value=True):
            with patch("cs2_vision_access.inference.cuda.nms._nms_gpu", side_effect=RuntimeError):
                boxes = np.array([[10, 10, 100, 100]], dtype=np.float32)
                scores = np.array([0.9], dtype=np.float32)
                result = nms(boxes, scores)
                self.assertEqual(result, [0])
