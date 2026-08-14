"""Shared helpers for detection-only ONNX segmenter backends (NanoDet, YOLOv10)."""

from __future__ import annotations

from collections.abc import Collection
from pathlib import Path

import numpy as np

from cs2_vision_access.config.data import coco80_class_names
from cs2_vision_access.model_manifest import ModelManifest, verify_model
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters.protocol import SegmenterError


def validate_detect_params(confidence: float, image_size: int) -> None:
    """Validate confidence and image_size for detection backends."""
    if not 0.0 < confidence <= 1.0:
        raise ValueError("confidence must be in (0, 1]")
    if image_size < 320 or image_size > 1920:
        raise ValueError("image_size must be in [320, 1920]")


def load_manifest_or_default(
    model_path: str | Path,
    manifest_path: str | Path | None,
    *,
    model_filename: str,
    origin: str,
    license_name: str,
) -> ModelManifest:
    """Load a verified manifest, or build a default COCO-80 detect manifest."""
    if manifest_path is not None and Path(manifest_path).is_file():
        _model, manifest = verify_model(model_path, manifest_path)
        return manifest

    return ModelManifest(
        schema_version=1,
        model_filename=model_filename,
        sha256="",
        task="detect",
        classes={i: name for i, name in enumerate(coco80_class_names())},
        origin=origin,
        license=license_name,
    )


def normalize_det_output(
    raw_out: np.ndarray,
    *,
    allow_7_cols: bool = False,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Normalize a detection tensor to ``(boxes, scores, class_ids)``.

    Expected row layout after normalization: ``[x1, y1, x2, y2, score, class_id]``.

    Args:
        raw_out: Model output tensor.
        allow_7_cols: When True (NanoDet), accept extra batch dims and an optional
            leading index column ``[batch, x1, y1, x2, y2, score, class_id]``.
            When False (YOLOv10), require exactly 6 columns after squeezing a
            leading batch dimension of size 1.
    """
    if allow_7_cols:
        # Common formats: [1, N, 6], [N, 6], [N, 7], [1, 1, N, 6]
        if raw_out.ndim == 4:
            raw_out = raw_out.squeeze(0)
        if raw_out.ndim == 3 and raw_out.shape[0] == 1:
            raw_out = raw_out[0]
        if raw_out.ndim == 3:
            raw_out = raw_out[0]

        if raw_out.ndim != 2 or raw_out.shape[1] < 6:
            raise SegmenterError(
                f"unexpected output shape {raw_out.shape}; expected [N, 6] or [1, N, 6]"
            )

        if raw_out.shape[1] == 7:
            raw_out = raw_out[:, 1:]
    else:
        if raw_out.ndim == 3 and raw_out.shape[0] == 1:
            raw_out = raw_out[0]

        if raw_out.ndim != 2 or raw_out.shape[1] != 6:
            raise SegmenterError(f"expected output shape [N, 6], got {raw_out.shape}")

    boxes = raw_out[:, :4]
    scores = raw_out[:, 4]
    class_ids = raw_out[:, 5]
    return boxes, scores, class_ids


def boxes_to_rectangle_masks(
    boxes: np.ndarray,
    scores: np.ndarray,
    class_ids: np.ndarray,
    *,
    frame_index: int,
    width: int,
    height: int,
    manifest: ModelManifest,
    allowed_ids: Collection[int],
    confidence: float,
) -> tuple[InstanceMask, ...]:
    """Filter detections and convert axis-aligned boxes to rectangle InstanceMasks.

    Boxes must already be in original frame pixel space (callers that letterbox
    must unproject before calling this helper).
    """
    mask = scores >= confidence
    boxes = boxes[mask]
    scores = scores[mask]
    class_ids = class_ids[mask]

    if len(scores) == 0:
        return ()

    class_id_list = class_ids.astype(int).tolist()
    confidences = scores.astype(float).tolist()

    predictions: list[InstanceMask] = []
    for class_id, score, box in zip(class_id_list, confidences, boxes, strict=True):
        if class_id not in manifest.classes:
            raise SegmenterError(f"model returned class id {class_id} absent from its manifest")
        if class_id not in allowed_ids:
            continue

        x1, y1, x2, y2 = (float(v) for v in box)
        x1 = float(np.clip(x1, 0.0, float(width)))
        y1 = float(np.clip(y1, 0.0, float(height)))
        x2 = float(np.clip(x2, 0.0, float(width)))
        y2 = float(np.clip(y2, 0.0, float(height)))

        if not (x2 > x1 and y2 > y1):
            continue

        polygon = ((x1, y1), (x2, y1), (x2, y2), (x1, y2))
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
