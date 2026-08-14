"""Optional RF-DETR segmentation backend (requires the ``rfdetr`` package).

Selected through :func:`~cs2_vision_access.segmenters.factory.create_segmenter`
with backend name ``rfdetr`` (alias ``rf-detr``).
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np

from cs2_vision_access.model_manifest import ModelManifest, verify_model
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters.protocol import (
    RFDETR_BACKEND,
    SegmenterError,
    resolve_class_ids,
)

_INSTALL_HINT = (
    "rfdetr is required for the rfdetr backend; "
    "install with: pip install 'cs2-vision-access[rfdetr]' "
    "or: pip install rfdetr"
)


class RfDetrSegmenter:
    """RF-DETR instance-segmentation adapter returning InstanceMasks."""

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

        try:
            import rfdetr as rfdetr_pkg
        except ImportError as error:
            raise SegmenterError(_INSTALL_HINT) from error

        model, manifest = verify_model(model_path, manifest_path)
        self.manifest = manifest
        self.allowed_ids = resolve_class_ids(manifest, class_names)
        self.confidence = confidence
        self.image_size = image_size
        self.device = device
        self.backend = RFDETR_BACKEND

        self._model = _load_rfdetr_model(rfdetr_pkg, model, device=device)

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        if frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
            raise ValueError("frame_bgr must have shape (height, width, 3)")

        height, width = frame_bgr.shape[:2]
        try:
            import cv2
        except ImportError as error:
            raise SegmenterError(
                "OpenCV is required for the rfdetr backend; install project dependencies"
            ) from error
        frame_rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
        try:
            detections = self._model.predict(
                frame_rgb,
                threshold=self.confidence,
                shape=(self.image_size, self.image_size),
                include_source_image=False,
            )
        except TypeError:
            try:
                detections = self._model.predict(frame_rgb, threshold=self.confidence)
            except Exception as error:
                raise SegmenterError(f"model inference failed: {error}") from error
        except Exception as error:
            raise SegmenterError(f"model inference failed: {error}") from error

        if detections is None:
            return ()
        if isinstance(detections, list):
            if len(detections) != 1:
                raise SegmenterError(f"expected one result for one frame, got {len(detections)}")
            detections = detections[0]

        return _detections_to_instance_masks(
            detections,
            frame_index=frame_index,
            width=width,
            height=height,
            allowed_ids=self.allowed_ids,
            manifest=self.manifest,
        )


def _load_rfdetr_model(rfdetr_pkg: Any, model_path: Path, *, device: str) -> Any:
    """Load a local checkpoint with the documented RF-DETR APIs."""
    path_str = str(model_path)
    try:
        from_checkpoint = getattr(rfdetr_pkg, "from_checkpoint", None)
        if callable(from_checkpoint):
            try:
                return from_checkpoint(path_str)
            except TypeError:
                return from_checkpoint(path_str, device=device)
        seg_cls = getattr(rfdetr_pkg, "RFDETRSegNano", None)
        if seg_cls is None:
            raise SegmenterError(
                "rfdetr package is installed but exposes neither from_checkpoint nor RFDETRSegNano"
            )
        try:
            return seg_cls(pretrain_weights=path_str)
        except TypeError:
            return seg_cls(pretrain_weights=path_str, device=device)
    except SegmenterError:
        raise
    except Exception as error:
        raise SegmenterError(
            f"could not load verified RF-DETR model from {path_str!r}: {error}"
        ) from error


def _detections_to_instance_masks(
    detections: Any,
    *,
    frame_index: int,
    width: int,
    height: int,
    allowed_ids: frozenset[int],
    manifest: ModelManifest,
) -> tuple[InstanceMask, ...]:
    """Convert supervision-style Detections (with optional masks) to InstanceMask."""
    class_ids_raw = getattr(detections, "class_id", None)
    confidences_raw = getattr(detections, "confidence", None)
    masks_raw = getattr(detections, "mask", None)

    if class_ids_raw is None and confidences_raw is None and masks_raw is None:
        return ()
    if class_ids_raw is None or confidences_raw is None:
        raise SegmenterError("backend returned boxes and masks inconsistently")

    class_ids = np.asarray(class_ids_raw, dtype=np.float64)
    confidences = np.asarray(confidences_raw, dtype=np.float64)
    if class_ids.ndim != 1 or confidences.ndim != 1:
        raise SegmenterError("backend returned non-vector classes or confidences")
    if not np.all(np.isfinite(class_ids)):
        raise SegmenterError("backend returned a non-finite class id")
    if not np.all(class_ids == np.floor(class_ids)):
        raise SegmenterError("backend returned a non-integral class id")
    if not np.all(np.isfinite(confidences)):
        raise SegmenterError("backend returned a non-finite confidence")
    if np.any((confidences < 0.0) | (confidences > 1.0)):
        raise SegmenterError("backend returned a confidence outside [0, 1]")

    n = len(class_ids)
    if n == 0:
        return ()
    if len(confidences) != n:
        raise SegmenterError("backend returned mismatched box, confidence, and mask counts")
    if masks_raw is None:
        raise SegmenterError("backend returned boxes and masks inconsistently")

    masks = np.asarray(masks_raw)
    if masks.ndim != 3 or masks.shape[0] != n:
        raise SegmenterError("backend returned mismatched box, confidence, and mask counts")

    class_id_list = class_ids.astype(int).tolist()
    confidence_list = confidences.astype(float).tolist()
    predictions: list[InstanceMask] = []
    for class_id, score, mask in zip(class_id_list, confidence_list, masks, strict=True):
        if class_id not in manifest.classes:
            raise SegmenterError(f"model returned class id {class_id} absent from its manifest")
        if class_id not in allowed_ids:
            continue
        polygon = _mask_to_polygon(mask, width=width, height=height)
        if polygon is None:
            continue
        predictions.append(
            InstanceMask(
                frame_index=frame_index,
                polygon=polygon,
                confidence=float(score),
                class_id=class_id,
                class_name=manifest.classes[class_id],
            )
        )
    return tuple(predictions)


def _mask_to_polygon(
    mask: np.ndarray, *, width: int, height: int
) -> tuple[tuple[float, float], ...] | None:
    """Largest external contour of a binary mask as a pixel-space polygon."""
    if mask.ndim != 2:
        raise SegmenterError("backend returned a malformed mask")
    from cs2_vision_access.segmenters._preprocessing import mask_to_polygon as _shared_polygon

    try:
        result = _shared_polygon(mask, epsilon=2.0, width=width, height=height)
    except ValueError as error:
        raise SegmenterError(str(error)) from error
    return result  # tuple or None
