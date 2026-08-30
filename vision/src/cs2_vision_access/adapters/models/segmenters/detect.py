"""Ultralytics ONNX detection backend (box → axis-aligned rectangle outlines).

Bootstrap / research path only: rectangles are not silhouettes.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, cast

import numpy as np

from cs2_vision_access.adapters.models.segmenters.protocol import (
    ULTRALYTICS_DETECT_BACKEND,
    SegmenterError,
    _require_bgr_frame,
    resolve_class_ids,
)
from cs2_vision_access.application.model_assets.manifest import verify_model
from cs2_vision_access.domain.predictions import InstanceMask


class UltralyticsOnnxDetector:
    """Ultralytics detect-task adapter returning rectangle InstanceMasks."""

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
        self.backend = ULTRALYTICS_DETECT_BACKEND

        try:
            from ultralytics import YOLO
        except ImportError as error:
            raise SegmenterError(
                "Ultralytics is required for ONNX result parsing; install the project dependencies"
            ) from error
        try:
            self._model = YOLO(str(model), task="detect")
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
        if result.boxes is None:
            return ()

        boxes = cast(Any, result.boxes)
        raw_class_ids = np.asarray(boxes.cls.detach().cpu().numpy(), dtype=np.float64)
        raw_confidences = np.asarray(boxes.conf.detach().cpu().numpy(), dtype=np.float64)
        raw_xyxy = np.asarray(boxes.xyxy.detach().cpu().numpy(), dtype=np.float64)
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

        n = len(raw_class_ids)
        if n == 0:
            return ()
        if len(raw_confidences) != n:
            raise SegmenterError("backend returned mismatched box, confidence, and class counts")
        if raw_xyxy.ndim != 2 or raw_xyxy.shape != (n, 4):
            raise SegmenterError("backend returned mismatched box, confidence, and class counts")
        if not np.all(np.isfinite(raw_xyxy)):
            raise SegmenterError("backend returned a non-finite box")

        height, width = frame_bgr.shape[:2]
        class_ids = raw_class_ids.astype(int).tolist()
        confidences = raw_confidences.astype(float).tolist()
        predictions: list[InstanceMask] = []
        for class_id, score, box in zip(class_ids, confidences, raw_xyxy, strict=True):
            if class_id not in self.manifest.classes:
                raise SegmenterError(f"model returned class id {class_id} absent from its manifest")
            x1, y1, x2, y2 = (float(v) for v in box)
            if not (x2 > x1 and y2 > y1):
                raise SegmenterError("backend returned a degenerate or inverted box")
            if x1 < 0.0 or y1 < 0.0 or x2 > float(width) or y2 > float(height):
                raise SegmenterError("backend returned a box outside the source frame")
            if class_id not in self.allowed_ids:
                continue
            polygon = ((x1, y1), (x2, y1), (x2, y2), (x1, y2))
            predictions.append(
                InstanceMask(
                    frame_index=frame_index,
                    polygon=polygon,
                    confidence=float(score),
                    class_id=class_id,
                    class_name=self.manifest.classes[class_id],
                )
            )
        return tuple(predictions)
