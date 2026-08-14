"""Tests for CUDA preprocessing helpers: letterbox, YOLO decode, mask polygons."""

from __future__ import annotations

import sys
import types
import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.inference.cuda.preprocess import (
    _letterbox_cpu,
    _yolo_decode_cpu,
    gpu_letterbox,
    gpu_mask_to_polygon,
    gpu_yolo_decode,
)
from cs2_vision_access.segmenters._preprocessing import mask_to_polygon


def _letterbox_cv2_module() -> types.ModuleType:
    cv2 = types.ModuleType("cv2")
    cv2.INTER_LINEAR = 1
    cv2.BORDER_CONSTANT = 0
    cv2.COLOR_BGR2RGB = 4

    def resize(img: np.ndarray, size: tuple[int, int], interpolation: object = None) -> np.ndarray:
        del interpolation
        tw, th = size
        ih, iw = img.shape[:2]
        if (tw, th) == (iw, ih):
            return img.copy()
        ys = (np.arange(th) * ih) // th
        xs = (np.arange(tw) * iw) // tw
        return img[ys][:, xs]

    def copy_make_border(
        src: np.ndarray,
        top: int,
        bottom: int,
        left: int,
        right: int,
        border_type: object,
        value: tuple[int, int, int] = (0, 0, 0),
    ) -> np.ndarray:
        del border_type
        h, w = src.shape[:2]
        out = np.full((h + top + bottom, w + left + right) + src.shape[2:], value, dtype=src.dtype)
        out[top : top + h, left : left + w] = src
        return out

    def cvt_color(img: np.ndarray, code: object) -> np.ndarray:
        del code
        return img[..., ::-1].copy()

    cv2.resize = resize
    cv2.copyMakeBorder = copy_make_border
    cv2.cvtColor = cvt_color
    return cv2


def _polygon_cv2_mock() -> MagicMock:
    cv2 = MagicMock()
    cv2.RETR_EXTERNAL = 0
    cv2.CHAIN_APPROX_SIMPLE = 2
    cv2.INTER_NEAREST = 0
    cv2.contourArea.return_value = 1024.0
    return cv2


SQUARE_CONTOUR = np.array([[[16, 16]], [[48, 16]], [[48, 48]], [[16, 48]]], dtype=np.int32)


def _shoelace_area(polygon: tuple[tuple[float, float], ...]) -> float:
    area = 0.0
    for i, (x, y) in enumerate(polygon):
        nx, ny = polygon[(i + 1) % len(polygon)]
        area += x * ny - y * nx
    return abs(area) / 2.0


class GpuMaskToPolygonAvailabilityTests(unittest.TestCase):
    def test_returns_none_when_cuda_unavailable(self) -> None:
        with patch(
            "cs2_vision_access.inference.cuda.preprocess.cuda_available",
            return_value=False,
        ):
            result = gpu_mask_to_polygon(np.zeros((32, 32), dtype=np.float32))
        self.assertIsNone(result)


class LetterboxCpuContractTests(unittest.TestCase):
    def test_square_letterbox_is_identity(self) -> None:
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        frame[:, :, 0] = 255  # B
        frame[:, :, 1] = 128  # G
        frame[:, :, 2] = 64  # R
        with patch.dict(sys.modules, {"cv2": _letterbox_cv2_module()}):
            batch, scale_x, scale_y, pad_left, pad_top = _letterbox_cpu(frame, 32)
        self.assertEqual(batch.shape, (1, 3, 32, 32))
        self.assertEqual(batch.dtype, np.float32)
        self.assertAlmostEqual(scale_x, 1.0)
        self.assertAlmostEqual(scale_y, 1.0)
        self.assertEqual(pad_left, 0)
        self.assertEqual(pad_top, 0)
        # BGR (255, 128, 64) becomes RGB (64, 128, 255), normalised to [0, 1].
        self.assertAlmostEqual(float(batch[0, 0, 0, 0]), 64 / 255.0, places=6)
        self.assertAlmostEqual(float(batch[0, 1, 0, 0]), 128 / 255.0, places=6)
        self.assertAlmostEqual(float(batch[0, 2, 0, 0]), 255 / 255.0, places=6)

    def test_letterbox_pads_and_reports_scale_factors(self) -> None:
        frame = np.zeros((48, 32, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": _letterbox_cv2_module()}):
            batch, scale_x, scale_y, pad_left, pad_top = _letterbox_cpu(frame, 64)
        self.assertEqual(batch.shape, (1, 3, 64, 64))
        self.assertEqual(pad_top, 0)
        self.assertEqual(pad_left, 10)
        self.assertAlmostEqual(scale_x, 32 / 43, places=6)
        self.assertAlmostEqual(scale_y, 48 / 64, places=6)


class YoloDecodeCpuContractTests(unittest.TestCase):
    def test_decode_math_and_confidence_filtering(self) -> None:
        raw = np.zeros((6, 3), dtype=np.float32)
        # Box 0: cx=320, cy=240, w=64, h=64 in 640-space on a 640x480 frame.
        raw[0, 0] = 320.0
        raw[1, 0] = 240.0
        raw[2, 0] = 64.0
        raw[3, 0] = 64.0
        raw[4, 0] = 0.8  # class 0
        raw[5, 0] = 0.2
        # Box 1: class 1 wins, box clips to frame width.
        raw[0, 1] = 640.0
        raw[1, 1] = 480.0
        raw[2, 1] = 128.0
        raw[3, 1] = 64.0
        raw[4, 1] = 0.1
        raw[5, 1] = 0.9
        # Box 2: below the confidence threshold.
        raw[4, 2] = 0.3
        raw[5, 2] = 0.2

        boxes, scores, class_ids = _yolo_decode_cpu(
            raw, num_classes=2, orig_w=640, orig_h=480, confidence=0.5
        )

        self.assertEqual(boxes.shape, (2, 4))
        np.testing.assert_array_almost_equal(boxes[0], [288.0, 156.0, 352.0, 204.0], decimal=4)
        np.testing.assert_array_almost_equal(boxes[1], [576.0, 336.0, 640.0, 384.0], decimal=4)
        np.testing.assert_array_almost_equal(scores, [0.8, 0.9])
        np.testing.assert_array_equal(class_ids, [0, 1])


def _handcrafted_raw() -> np.ndarray:
    raw = np.zeros((6, 3), dtype=np.float32)
    raw[0, 0] = 320.0
    raw[1, 0] = 240.0
    raw[2, 0] = 64.0
    raw[3, 0] = 64.0
    raw[4, 0] = 0.8
    raw[5, 0] = 0.2
    raw[0, 1] = 640.0
    raw[1, 1] = 480.0
    raw[2, 1] = 128.0
    raw[3, 1] = 64.0
    raw[4, 1] = 0.1
    raw[5, 1] = 0.9
    raw[4, 2] = 0.3
    raw[5, 2] = 0.2
    return raw


class GpuYoloDecodeCpuFallbackTests(unittest.TestCase):
    def test_falls_back_to_cpu_when_cuda_unavailable(self) -> None:
        raw = _handcrafted_raw()
        with patch(
            "cs2_vision_access.inference.cuda.preprocess.cuda_available",
            return_value=False,
        ):
            boxes, scores, class_ids = gpu_yolo_decode(raw, 2, 640, 480, confidence=0.5)
        expected_b, expected_s, expected_c = _yolo_decode_cpu(raw, 2, 640, 480, confidence=0.5)
        self.assertEqual(boxes.shape[1], 4)
        np.testing.assert_array_equal(boxes, expected_b)
        np.testing.assert_array_almost_equal(scores, expected_s)
        np.testing.assert_array_equal(class_ids, expected_c)


class GpuLetterboxCpuFallbackTests(unittest.TestCase):
    def test_falls_back_to_cpu_and_normalizes(self) -> None:
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        frame[:, :, 0] = 255
        with (
            patch(
                "cs2_vision_access.inference.cuda.preprocess.cuda_available",
                return_value=False,
            ),
            patch.dict(sys.modules, {"cv2": _letterbox_cv2_module()}),
        ):
            batch, scale_x, scale_y, pad_left, pad_top = gpu_letterbox(frame, 32)
        self.assertEqual(batch.shape, (1, 3, 32, 32))
        self.assertGreaterEqual(float(batch.min()), 0.0)
        self.assertLessEqual(float(batch.max()), 1.0)
        self.assertAlmostEqual(scale_x, 1.0)
        self.assertAlmostEqual(scale_y, 1.0)
        self.assertEqual(pad_left, 0)
        self.assertEqual(pad_top, 0)


class MaskToPolygonCpuPathTests(unittest.TestCase):
    def test_filled_square_returns_polygon_with_square_area(self) -> None:
        mask = np.zeros((64, 64), dtype=np.uint8)
        mask[16:48, 16:48] = 1
        cv2 = _polygon_cv2_mock()
        cv2.findContours.return_value = ([SQUARE_CONTOUR], None)
        cv2.approxPolyDP.return_value = SQUARE_CONTOUR.astype(np.float32)
        with (
            patch("cs2_vision_access.inference.cuda.utils.cuda_available", return_value=False),
            patch.dict(sys.modules, {"cv2": cv2}),
        ):
            polygon = mask_to_polygon(mask)
        self.assertIsNotNone(polygon)
        assert polygon is not None
        self.assertGreaterEqual(len(polygon), 3)
        self.assertAlmostEqual(_shoelace_area(polygon), 32 * 32, delta=1.0)

    def test_empty_mask_returns_empty_tuple(self) -> None:
        mask = np.zeros((32, 32), dtype=np.uint8)
        cv2 = _polygon_cv2_mock()
        cv2.findContours.return_value = ([], None)
        with (
            patch("cs2_vision_access.inference.cuda.utils.cuda_available", return_value=False),
            patch.dict(sys.modules, {"cv2": cv2}),
        ):
            polygon = mask_to_polygon(mask)
        self.assertEqual(polygon, ())

    def test_full_mask_returns_polygon(self) -> None:
        mask = np.ones((32, 32), dtype=np.uint8)
        cv2 = _polygon_cv2_mock()
        cv2.findContours.return_value = ([SQUARE_CONTOUR], None)
        cv2.approxPolyDP.return_value = SQUARE_CONTOUR.astype(np.float32)
        with (
            patch("cs2_vision_access.inference.cuda.utils.cuda_available", return_value=False),
            patch.dict(sys.modules, {"cv2": cv2}),
        ):
            polygon = mask_to_polygon(mask)
        self.assertIsNotNone(polygon)
        assert polygon is not None
        self.assertGreaterEqual(len(polygon), 3)

    def test_width_height_resize_path_resizes_before_contour(self) -> None:
        mask = np.zeros((64, 64), dtype=np.uint8)
        mask[16:48, 16:48] = 1
        cv2 = _polygon_cv2_mock()
        cv2.findContours.return_value = ([SQUARE_CONTOUR], None)
        cv2.approxPolyDP.return_value = SQUARE_CONTOUR.astype(np.float32)
        with (
            patch("cs2_vision_access.inference.cuda.utils.cuda_available", return_value=False),
            patch.dict(sys.modules, {"cv2": cv2}),
        ):
            polygon = mask_to_polygon(mask, width=128, height=128)
        self.assertIsNotNone(polygon)
        assert polygon is not None
        self.assertGreaterEqual(len(polygon), 3)
        cv2.resize.assert_called_once()
        args, kwargs = cv2.resize.call_args
        self.assertEqual(tuple(args[1]), (128, 128))
        self.assertEqual(kwargs["interpolation"], cv2.INTER_NEAREST)


class PreprocessImportSafetyTests(unittest.TestCase):
    def test_preprocess_imports_without_cupy(self) -> None:
        had_cupy = "cupy" in sys.modules
        import cs2_vision_access.inference.cuda.preprocess as preprocess

        self.assertTrue(callable(preprocess.gpu_mask_to_polygon))
        if not had_cupy:
            self.assertNotIn("cupy", sys.modules)


if __name__ == "__main__":
    unittest.main()
