"""GPU-accelerated YOLO output decode.

Extracted from ``pipeline.py`` to keep the pipeline module focused on
orchestration.
"""

from __future__ import annotations

from typing import Any

from cs2_vision_access.inference.cuda.memory import get_pool
from cs2_vision_access.inference.cuda.utils import cuda_available


def decode_gpu(
    raw: Any,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
) -> tuple[Any, Any, Any]:
    """Decode raw Ultralytics ONNX output on GPU with pre-allocated buffers.

    Returns:
        ``(boxes, scores, class_ids)`` — CuPy arrays on GPU.
    """
    import cupy as cp

    pool = get_pool()
    num_preds = raw.shape[-1]
    scale_fw = cp.float32(det_size)

    if pool is not None:
        boxes_buf = pool.boxes[:num_preds]
        scores_buf = pool.scores[:num_preds]
        ids_buf = pool.class_ids[:num_preds]
    else:
        boxes_buf = cp.empty((num_preds, 4), dtype=cp.float32)
        scores_buf = cp.empty(num_preds, dtype=cp.float32)
        ids_buf = cp.empty(num_preds, dtype=cp.int32)

    if raw.ndim == 3:
        raw = raw[0]

    cx = raw[0] / scale_fw * orig_w
    cy = raw[1] / scale_fw * orig_h
    w = raw[2] / scale_fw * orig_w
    h = raw[3] / scale_fw * orig_h

    boxes_buf[:, 0] = cp.maximum(0, cx - w / 2)
    boxes_buf[:, 1] = cp.maximum(0, cy - h / 2)
    boxes_buf[:, 2] = cp.minimum(orig_w, cx + w / 2)
    boxes_buf[:, 3] = cp.minimum(orig_h, cy + h / 2)

    cls_data = raw[4 : 4 + num_classes]
    ids_buf[:num_preds] = cls_data.argmax(axis=0)
    scores_buf[:num_preds] = cls_data[ids_buf[:num_preds], cp.arange(num_preds)]

    return boxes_buf[:num_preds], scores_buf[:num_preds], ids_buf[:num_preds]


def decode_cpu(
    raw: Any,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
) -> tuple[Any, Any, Any]:
    """Reference CPU YOLO decode.

    Returns:
        ``(boxes, scores, class_ids)`` — NumPy arrays.
    """
    import numpy as np

    if hasattr(raw, "ndim") and raw.ndim == 3:
        raw = raw[0]

    num_preds = raw.shape[1]
    scale = float(det_size)

    cx = raw[0] / scale * orig_w
    cy = raw[1] / scale * orig_h
    w = raw[2] / scale * orig_w
    h = raw[3] / scale * orig_h

    x1 = (cx - w / 2).clip(0, orig_w)
    y1 = (cy - h / 2).clip(0, orig_h)
    x2 = (cx + w / 2).clip(0, orig_w)
    y2 = (cy + h / 2).clip(0, orig_h)

    boxes = np.stack([x1, y1, x2, y2], axis=1)

    cls_data = raw[4 : 4 + num_classes]
    class_ids = cls_data.argmax(axis=0)
    scores = cls_data[class_ids, np.arange(num_preds)]

    return boxes, scores, class_ids


def decode(
    raw: Any,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
) -> tuple[Any, Any, Any]:
    """Decode raw ONNX output on GPU when available, CPU otherwise."""
    if cuda_available() and hasattr(raw, "__cuda_array_interface__"):
        try:
            return decode_gpu(raw, num_classes, orig_w, orig_h, det_size)
        except (ImportError, RuntimeError):
            # A missing CUDA backend or failed kernel falls back to NumPy.
            pass
    return decode_cpu(raw, num_classes, orig_w, orig_h, det_size)
