from __future__ import annotations

import sys
import unittest
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.model_manifest import ModelManifest, ModelManifestError
from cs2_vision_access.segmenters import (
    DEFAULT_SEGMENTER_BACKEND,
    RFDETR_BACKEND,
    ULTRALYTICS_DETECT_BACKEND,
    SegmenterError,
    UltralyticsOnnxSegmenter,
    create_segmenter,
    normalize_segmenter_backend,
)
from cs2_vision_access.segmenters.detect import UltralyticsOnnxDetector
from cs2_vision_access.segmenters.rfdetr import RfDetrSegmenter


class _Tensor:
    def __init__(self, values: list[float]) -> None:
        self.values = np.asarray(values, dtype=np.float64)

    def detach(self) -> _Tensor:
        return self

    def cpu(self) -> _Tensor:
        return self

    def numpy(self) -> np.ndarray:
        return self.values


class _Model:
    def __init__(self, result: object) -> None:
        self.result = result

    def predict(self, **_arguments: object) -> list[object]:
        return [self.result]


def _subject(
    *,
    class_ids: list[float],
    confidences: list[float],
    polygons: list[np.ndarray] | None,
    allowed_ids: frozenset[int] = frozenset({0}),
) -> UltralyticsOnnxSegmenter:
    segmenter = UltralyticsOnnxSegmenter.__new__(UltralyticsOnnxSegmenter)
    segmenter.manifest = ModelManifest(
        schema_version=1,
        model_filename="fixture.onnx",
        sha256="0" * 64,
        task="instance-segmentation",
        classes={0: "player"},
        origin="test",
        license="test-only",
    )
    segmenter.allowed_ids = allowed_ids
    segmenter.confidence = 0.45
    segmenter.image_size = 640
    segmenter.device = "cpu"
    boxes = SimpleNamespace(cls=_Tensor(class_ids), conf=_Tensor(confidences))
    masks = None if polygons is None else SimpleNamespace(xy=polygons)
    segmenter._model = _Model(SimpleNamespace(boxes=boxes, masks=masks))
    return segmenter


class SegmenterResultContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.frame = np.zeros((32, 48, 3), dtype=np.uint8)
        self.valid_polygon = np.asarray(
            [[1.0, 1.0], [20.0, 1.0], [10.0, 24.0]],
            dtype=np.float32,
        )

    def test_valid_mask_is_bound_to_current_frame(self) -> None:
        segmenter = _subject(
            class_ids=[0.0],
            confidences=[0.8],
            polygons=[self.valid_polygon],
        )

        predictions = segmenter.predict(self.frame, frame_index=7)

        self.assertEqual(len(predictions), 1)
        self.assertEqual(predictions[0].frame_index, 7)
        self.assertEqual(predictions[0].class_name, "player")

    def test_boxes_without_masks_fail_closed(self) -> None:
        segmenter = _subject(class_ids=[0.0], confidences=[0.8], polygons=None)

        with self.assertRaisesRegex(SegmenterError, "inconsistently"):
            segmenter.predict(self.frame, frame_index=0)

    def test_empty_boxes_without_mask_object_are_no_detections(self) -> None:
        segmenter = _subject(class_ids=[], confidences=[], polygons=None)

        self.assertEqual(segmenter.predict(self.frame, frame_index=0), ())

    def test_unselected_out_of_manifest_class_fails_closed(self) -> None:
        segmenter = _subject(
            class_ids=[1.0],
            confidences=[0.8],
            polygons=[self.valid_polygon],
            allowed_ids=frozenset({0}),
        )

        with self.assertRaisesRegex(SegmenterError, "absent from its manifest"):
            segmenter.predict(self.frame, frame_index=0)

    def test_nonfinite_polygon_fails_closed(self) -> None:
        polygon = self.valid_polygon.copy()
        polygon[1, 0] = np.nan
        segmenter = _subject(
            class_ids=[0.0],
            confidences=[0.8],
            polygons=[polygon],
        )

        with self.assertRaisesRegex(SegmenterError, "non-finite mask"):
            segmenter.predict(self.frame, frame_index=0)

    def test_nonintegral_class_and_invalid_confidence_fail_closed(self) -> None:
        cases = (
            ([0.5], [0.8], "non-integral class"),
            ([0.0], [float("inf")], "non-finite confidence"),
            ([0.0], [1.1], "outside"),
        )
        for class_ids, confidences, message in cases:
            with self.subTest(message=message):
                segmenter = _subject(
                    class_ids=class_ids,
                    confidences=confidences,
                    polygons=[self.valid_polygon],
                )
                with self.assertRaisesRegex(SegmenterError, message):
                    segmenter.predict(self.frame, frame_index=0)


class SegmenterFactoryTests(unittest.TestCase):
    def test_default_backend_name_is_ultralytics_onnx(self) -> None:
        self.assertEqual(DEFAULT_SEGMENTER_BACKEND, "ultralytics-onnx")

    def test_unknown_backend_fails_closed_before_model_io(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown or unsupported"):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend="not-a-backend",
            )

    def test_blank_backend_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown or unsupported"):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend="   ",
            )

    def test_casefold_valid_backend_passes_name_gate(self) -> None:
        # Case-insensitive match should reach model validation, not the name gate.
        with self.assertRaises(ModelManifestError):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend="ULTRALYTICS-ONNX",
            )

    def test_supported_backend_reaches_model_validation(self) -> None:
        with self.assertRaises(ModelManifestError):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend=DEFAULT_SEGMENTER_BACKEND,
            )

    def test_rf_detr_alias_normalizes_to_rfdetr(self) -> None:
        self.assertEqual(normalize_segmenter_backend("RF-DETR"), RFDETR_BACKEND)
        self.assertEqual(normalize_segmenter_backend(" rfdetr "), RFDETR_BACKEND)

    def test_yolo_detect_alias_normalizes_to_ultralytics_detect(self) -> None:
        self.assertEqual(normalize_segmenter_backend("YOLO-DETECT"), ULTRALYTICS_DETECT_BACKEND)
        self.assertEqual(
            normalize_segmenter_backend(" ultralytics-detect "),
            ULTRALYTICS_DETECT_BACKEND,
        )

    def test_ultralytics_detect_reaches_model_validation(self) -> None:
        with self.assertRaises(ModelManifestError):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend=ULTRALYTICS_DETECT_BACKEND,
            )

    def test_yolo_detect_alias_reaches_model_validation(self) -> None:
        with self.assertRaises(ModelManifestError):
            create_segmenter(
                "missing.onnx",
                "missing.json",
                backend="yolo-detect",
            )


class RfDetrSegmenterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.frame = np.zeros((32, 48, 3), dtype=np.uint8)

    def test_rfdetr_without_package_fails_closed(self) -> None:
        real_import = __import__

        def _block_rfdetr(name: str, *args: object, **kwargs: object) -> object:
            if name == "rfdetr" or name.startswith("rfdetr."):
                raise ImportError("No module named 'rfdetr'")
            return real_import(name, *args, **kwargs)  # type: ignore[arg-type]

        with patch("builtins.__import__", side_effect=_block_rfdetr):
            # Drop any previously imported rfdetr stub from this process.
            with patch.dict(sys.modules, {"rfdetr": None}):
                with self.assertRaisesRegex(SegmenterError, r"pip install.*rfdetr"):
                    create_segmenter(
                        "missing.onnx",
                        "missing.json",
                        backend="rfdetr",
                    )

    def test_rf_detr_alias_without_package_fails_closed(self) -> None:
        real_import = __import__

        def _block_rfdetr(name: str, *args: object, **kwargs: object) -> object:
            if name == "rfdetr" or name.startswith("rfdetr."):
                raise ImportError("No module named 'rfdetr'")
            return real_import(name, *args, **kwargs)  # type: ignore[arg-type]

        with patch("builtins.__import__", side_effect=_block_rfdetr):
            with patch.dict(sys.modules, {"rfdetr": None}):
                with self.assertRaisesRegex(SegmenterError, r"pip install.*rfdetr"):
                    create_segmenter(
                        "missing.onnx",
                        "missing.json",
                        backend="rf-detr",
                    )

    def test_mocked_predict_returns_instance_mask(self) -> None:
        segmenter = RfDetrSegmenter.__new__(RfDetrSegmenter)
        segmenter.manifest = ModelManifest(
            schema_version=1,
            model_filename="fixture.onnx",
            sha256="0" * 64,
            task="instance-segmentation",
            classes={0: "player"},
            origin="test",
            license="test-only",
        )
        segmenter.allowed_ids = frozenset({0})
        segmenter.confidence = 0.45
        segmenter.image_size = 640
        segmenter.device = "cpu"
        segmenter.backend = RFDETR_BACKEND

        mask = np.zeros((32, 48), dtype=np.uint8)
        mask[4:20, 8:30] = 1
        detections = SimpleNamespace(
            class_id=np.asarray([0], dtype=np.int64),
            confidence=np.asarray([0.91], dtype=np.float64),
            mask=np.stack([mask]),
        )
        model = MagicMock()
        model.predict.return_value = detections
        segmenter._model = model

        contour = np.asarray(
            [[[8, 4]], [[30, 4]], [[30, 20]], [[8, 20]]],
            dtype=np.int32,
        )

        def _approx_poly_dp(points: np.ndarray, _epsilon: float, closed: bool = True) -> np.ndarray:
            # Match OpenCV: return (N, 1, 2) so callers can index p[0][0], p[0][1].
            arr = np.asarray(points, dtype=np.float32).reshape(-1, 1, 2)
            return arr

        fake_cv2 = SimpleNamespace(
            COLOR_BGR2RGB=4,
            RETR_EXTERNAL=0,
            CHAIN_APPROX_SIMPLE=2,
            INTER_NEAREST=0,
            cvtColor=lambda image, _code: image,
            resize=lambda image, size, interpolation=None: image,
            findContours=lambda *_a, **_k: ([contour], None),
            contourArea=lambda c: float(len(c)),
            approxPolyDP=_approx_poly_dp,
        )
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            predictions = segmenter.predict(self.frame, frame_index=4)

        self.assertEqual(len(predictions), 1)
        pred = predictions[0]
        self.assertEqual(pred.frame_index, 4)
        self.assertEqual(pred.class_id, 0)
        self.assertEqual(pred.class_name, "player")
        self.assertAlmostEqual(pred.confidence, 0.91)
        self.assertGreaterEqual(len(pred.polygon), 3)

    def test_mocked_empty_detections_are_no_masks(self) -> None:
        segmenter = RfDetrSegmenter.__new__(RfDetrSegmenter)
        segmenter.manifest = ModelManifest(
            schema_version=1,
            model_filename="fixture.onnx",
            sha256="0" * 64,
            task="instance-segmentation",
            classes={0: "player"},
            origin="test",
            license="test-only",
        )
        segmenter.allowed_ids = frozenset({0})
        segmenter.confidence = 0.45
        segmenter.image_size = 640
        segmenter.device = "cpu"
        model = MagicMock()
        model.predict.return_value = SimpleNamespace(
            class_id=np.asarray([], dtype=np.int64),
            confidence=np.asarray([], dtype=np.float64),
            mask=None,
        )
        segmenter._model = model

        fake_cv2 = SimpleNamespace(
            COLOR_BGR2RGB=4,
            cvtColor=lambda image, _code: image,
        )
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            self.assertEqual(segmenter.predict(self.frame, frame_index=0), ())


class _BoxTensor:
    """Minimal tensor stand-in supporting 1-D class/conf and 2-D xyxy."""

    def __init__(self, values: list[float] | list[list[float]]) -> None:
        self.values = np.asarray(values, dtype=np.float64)

    def detach(self) -> _BoxTensor:
        return self

    def cpu(self) -> _BoxTensor:
        return self

    def numpy(self) -> np.ndarray:
        return self.values


def _detect_subject(
    *,
    class_ids: list[float],
    confidences: list[float],
    xyxy: list[list[float]] | None,
    allowed_ids: frozenset[int] = frozenset({0}),
) -> UltralyticsOnnxDetector:
    detector = UltralyticsOnnxDetector.__new__(UltralyticsOnnxDetector)
    detector.manifest = ModelManifest(
        schema_version=1,
        model_filename="fixture.onnx",
        sha256="0" * 64,
        task="instance-segmentation",
        classes={0: "player"},
        origin="test",
        license="test-only",
    )
    detector.allowed_ids = allowed_ids
    detector.confidence = 0.45
    detector.image_size = 640
    detector.device = "cpu"
    detector.backend = ULTRALYTICS_DETECT_BACKEND
    if xyxy is None:
        boxes = None
    else:
        boxes = SimpleNamespace(
            cls=_BoxTensor(class_ids),
            conf=_BoxTensor(confidences),
            xyxy=_BoxTensor(xyxy),
        )
    detector._model = _Model(SimpleNamespace(boxes=boxes))
    return detector


class UltralyticsDetectBackendTests(unittest.TestCase):
    def setUp(self) -> None:
        self.frame = np.zeros((32, 48, 3), dtype=np.uint8)
        self.valid_xyxy = [1.0, 2.0, 20.0, 28.0]

    def test_box_becomes_axis_aligned_rectangle_mask(self) -> None:
        detector = _detect_subject(
            class_ids=[0.0],
            confidences=[0.87],
            xyxy=[self.valid_xyxy],
        )

        predictions = detector.predict(self.frame, frame_index=5)

        self.assertEqual(len(predictions), 1)
        pred = predictions[0]
        self.assertEqual(pred.frame_index, 5)
        self.assertEqual(pred.class_id, 0)
        self.assertEqual(pred.class_name, "player")
        self.assertAlmostEqual(pred.confidence, 0.87)
        x1, y1, x2, y2 = self.valid_xyxy
        self.assertEqual(
            pred.polygon,
            ((x1, y1), (x2, y1), (x2, y2), (x1, y2)),
        )

    def test_empty_boxes_are_no_detections(self) -> None:
        detector = _detect_subject(class_ids=[], confidences=[], xyxy=[])
        self.assertEqual(detector.predict(self.frame, frame_index=0), ())

    def test_none_boxes_are_no_detections(self) -> None:
        detector = _detect_subject(class_ids=[], confidences=[], xyxy=None)
        self.assertEqual(detector.predict(self.frame, frame_index=0), ())

    def test_unselected_class_is_filtered(self) -> None:
        detector = UltralyticsOnnxDetector.__new__(UltralyticsOnnxDetector)
        detector.manifest = ModelManifest(
            schema_version=1,
            model_filename="fixture.onnx",
            sha256="0" * 64,
            task="instance-segmentation",
            classes={0: "player", 1: "weapon"},
            origin="test",
            license="test-only",
        )
        detector.allowed_ids = frozenset({0})
        detector.confidence = 0.45
        detector.image_size = 640
        detector.device = "cpu"
        boxes = SimpleNamespace(
            cls=_BoxTensor([1.0]),
            conf=_BoxTensor([0.9]),
            xyxy=_BoxTensor([self.valid_xyxy]),
        )
        detector._model = _Model(SimpleNamespace(boxes=boxes))

        self.assertEqual(detector.predict(self.frame, frame_index=0), ())

    def test_absent_manifest_class_fails_closed(self) -> None:
        detector = _detect_subject(
            class_ids=[1.0],
            confidences=[0.8],
            xyxy=[self.valid_xyxy],
            allowed_ids=frozenset({0}),
        )
        with self.assertRaisesRegex(SegmenterError, "absent from its manifest"):
            detector.predict(self.frame, frame_index=0)

    def test_degenerate_box_fails_closed(self) -> None:
        detector = _detect_subject(
            class_ids=[0.0],
            confidences=[0.8],
            xyxy=[[10.0, 10.0, 10.0, 20.0]],
        )
        with self.assertRaisesRegex(SegmenterError, "degenerate or inverted"):
            detector.predict(self.frame, frame_index=0)

    def test_box_outside_frame_fails_closed(self) -> None:
        detector = _detect_subject(
            class_ids=[0.0],
            confidences=[0.8],
            xyxy=[[1.0, 2.0, 100.0, 28.0]],
        )
        with self.assertRaisesRegex(SegmenterError, "outside the source frame"):
            detector.predict(self.frame, frame_index=0)

    def test_nonfinite_confidence_fails_closed(self) -> None:
        detector = _detect_subject(
            class_ids=[0.0],
            confidences=[float("nan")],
            xyxy=[self.valid_xyxy],
        )
        with self.assertRaisesRegex(SegmenterError, "non-finite confidence"):
            detector.predict(self.frame, frame_index=0)


if __name__ == "__main__":
    unittest.main()
