"""Ultralytics ONNX segmentation backend.

This is the default segmenter backend — loads a checksum-verified ONNX model
via Ultralytics ``YOLO(..., task="segment")`` and parses mask polygons.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from cs2_vision_access.model_manifest import verify_model
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters.protocol import (
    DEFAULT_SEGMENTER_BACKEND,
    SegmenterError,
    _require_bgr_frame,
    resolve_class_ids,
)


class UltralyticsOnnxSegmenter:
    """Ultralytics result parsing with an ONNX-only model boundary."""

    def __init__(
        self,
        model_path: str | Path,
        manifest_path: str | Path,
        *,
        class_names: tuple[str, ...] | None,
        confidence: float,
        image_size: int,
        device: str,
    ) -> None:
        if not 0.0 < confidence <= 1.0:
            raise ValueError("confidence must be in (0, 1]")
        if image_size < 320 or image_size > 1920:
            raise ValueError("image_size must be in [320, 1920]")
        model, manifest = verify_model(model_path, manifest_path)
        self.manifest = manifest
        self.allowed_ids = resolve_class_ids(manifest, class_names)
        self.confidence = confidence
        self.image_size = image_size
        self.device = device
        self.backend = DEFAULT_SEGMENTER_BACKEND

        try:
            from ultralytics import YOLO
        except ImportError as error:
            raise SegmenterError(
                "Ultralytics is required for ONNX result parsing; install the project dependencies"
            ) from error
        try:
            self._model = YOLO(str(model), task="segment")
        except Exception as error:
            raise SegmenterError(f"could not load verified ONNX model: {error}") from error

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        _require_bgr_frame(frame_bgr, frame_index)
        try:
            results = self._model.predict(
                source=frame_bgr,
                conf=self.confidence,
                imgsz=self.image_size,
                device=self.device,
                verbose=False,
            )
        except Exception as error:
            raise SegmenterError(f"model inference failed: {error}") from error

        if len(results) != 1:
            raise SegmenterError(f"expected one result for one frame, got {len(results)}")
        result = results[0]
        if result.boxes is None and result.masks is None:
            return ()
        if result.boxes is None:
            raise SegmenterError("backend returned boxes and masks inconsistently")

        raw_class_ids = np.asarray(result.boxes.cls.detach().cpu().numpy(), dtype=np.float64)
        raw_confidences = np.asarray(result.boxes.conf.detach().cpu().numpy(), dtype=np.float64)
        if raw_class_ids.ndim != 1 or raw_confidences.ndim != 1:
            raise SegmenterError("backend returned non-vector classes or confidences")
        if not np.all(np.isfinite(raw_class_ids)):
            raise SegmenterError("backend returned a non-finite class id")
        if not np.all(raw_class_ids == np.floor(raw_class_ids)):
            raise SegmenterError("backend returned a non-integral class id")
        if not np.all(np.isfinite(raw_confidences)):
            raise SegmenterError("backend returned a non-finite confidence")
        if np.any((raw_confidences < 0.0) | (raw_confidences > 1.0)):
            raise SegmenterError("backend returned a confidence outside [0, 1]")
        if result.masks is None:
            if len(raw_class_ids) == len(raw_confidences) == 0:
                return ()
            raise SegmenterError("backend returned boxes and masks inconsistently")

        class_ids = raw_class_ids.astype(int).tolist()
        confidences = raw_confidences.astype(float).tolist()
        polygons = result.masks.xy
        if not (len(class_ids) == len(confidences) == len(polygons)):
            raise SegmenterError("backend returned mismatched box, confidence, and mask counts")

        height, width = frame_bgr.shape[:2]
        predictions: list[InstanceMask] = []
        for class_id, score, polygon in zip(class_ids, confidences, polygons, strict=True):
            if class_id not in self.manifest.classes:
                raise SegmenterError(f"model returned class id {class_id} absent from its manifest")
            points = np.asarray(polygon, dtype=np.float32)
            if points.ndim != 2 or points.shape[1:] != (2,) or len(points) < 3:
                raise SegmenterError("backend returned a malformed mask polygon")
            if not np.all(np.isfinite(points)):
                raise SegmenterError("backend returned a non-finite mask polygon")
            if np.any(
                (points[:, 0] < 0.0)
                | (points[:, 0] > float(width))
                | (points[:, 1] < 0.0)
                | (points[:, 1] > float(height))
            ):
                raise SegmenterError("backend returned a mask outside the source frame")
            if class_id not in self.allowed_ids:
                continue
            predictions.append(
                InstanceMask(
                    frame_index=frame_index,
                    polygon=tuple((float(x), float(y)) for x, y in points),
                    confidence=float(score),
                    class_id=class_id,
                    class_name=self.manifest.classes[class_id],
                )
            )
        return tuple(predictions)
