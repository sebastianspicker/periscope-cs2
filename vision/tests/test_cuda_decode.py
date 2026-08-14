from __future__ import annotations

import unittest
from unittest.mock import patch

import numpy as np

from cs2_vision_access.inference.cuda.decode import decode, decode_cpu


class CudaDecodeCpuTests(unittest.TestCase):
    def _make_raw(self, num_classes: int = 4, num_preds: int = 8400) -> np.ndarray:
        raw = np.random.randn(4 + num_classes, num_preds).astype(np.float32)
        raw[0] = np.random.uniform(0, 640, num_preds).astype(np.float32)
        raw[1] = np.random.uniform(0, 640, num_preds).astype(np.float32)
        raw[2] = np.random.uniform(0, 100, num_preds).astype(np.float32)
        raw[3] = np.random.uniform(0, 100, num_preds).astype(np.float32)
        for c in range(num_classes):
            raw[4 + c] = np.random.uniform(0, 0.1, num_preds).astype(np.float32)
        raw[4] = np.random.uniform(0.5, 0.9, num_preds).astype(np.float32)
        return raw

    def test_decode_cpu_returns_correct_shape(self) -> None:
        raw = self._make_raw()
        boxes, scores, class_ids = decode_cpu(raw, num_classes=4, orig_w=1920, orig_h=1080)
        self.assertEqual(boxes.shape, (8400, 4))
        self.assertEqual(scores.shape, (8400,))
        self.assertEqual(class_ids.shape, (8400,))
        self.assertEqual(boxes.dtype, np.float32)
        self.assertEqual(class_ids.dtype, np.int64)

    def test_decode_cpu_clips_boxes_to_image_bounds(self) -> None:
        raw = self._make_raw()
        raw[2] = 9999
        boxes, scores, class_ids = decode_cpu(raw, num_classes=4, orig_w=1920, orig_h=1080)
        self.assertTrue((boxes[:, 0] >= 0).all())
        self.assertTrue((boxes[:, 1] >= 0).all())
        self.assertTrue((boxes[:, 2] <= 1920).all())
        self.assertTrue((boxes[:, 3] <= 1080).all())

    def test_decode_cpu_class_ids_match_highest_score(self) -> None:
        num_classes = 4
        raw = np.zeros((4 + num_classes, 100), dtype=np.float32)
        raw[4] = 0.8
        raw[5] = 0.2
        raw[2] = 20
        raw[3] = 20
        raw[0] = 320
        raw[1] = 320
        boxes, scores, class_ids = decode_cpu(raw, num_classes=4, orig_w=640, orig_h=640)
        self.assertTrue((class_ids == 0).all())
        self.assertTrue((scores == 0.8).all())

    def test_decode_cpu_with_3d_input(self) -> None:
        raw_3d = np.expand_dims(self._make_raw(), axis=0)
        boxes_3d, scores_3d, ids_3d = decode_cpu(raw_3d, num_classes=4, orig_w=1920, orig_h=1080)
        raw_2d = raw_3d[0]
        boxes_2d, scores_2d, ids_2d = decode_cpu(raw_2d, num_classes=4, orig_w=1920, orig_h=1080)
        np.testing.assert_array_equal(boxes_3d, boxes_2d)
        np.testing.assert_array_equal(scores_3d, scores_2d)
        np.testing.assert_array_equal(ids_3d, ids_2d)


class CudaDecodePublicApiTests(unittest.TestCase):
    def test_decode_cpu_fallback_when_cuda_unavailable(self) -> None:
        with patch("cs2_vision_access.inference.cuda.decode.cuda_available", return_value=False):
            raw = np.random.randn(8, 100).astype(np.float32)
            boxes, scores, class_ids = decode(raw, num_classes=4, orig_w=640, orig_h=640)
            self.assertEqual(boxes.shape, (100, 4))

    def test_decode_gpu_fallback_on_exception(self) -> None:
        with patch("cs2_vision_access.inference.cuda.decode.cuda_available", return_value=True):
            raw = np.random.randn(8, 100).astype(np.float32)
            boxes, scores, class_ids = decode(raw, num_classes=4, orig_w=640, orig_h=640)
            self.assertEqual(boxes.shape, (100, 4))
