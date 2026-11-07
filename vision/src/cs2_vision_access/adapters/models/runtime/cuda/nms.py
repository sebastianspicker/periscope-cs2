"""Non-Maximum Suppression, greedy NumPy implementation.

Greedy NMS is sequential (each kept box suppresses later ones), so it runs on
the host.  CuPy inputs are copied to the host once; the input is already
confidence-filtered, so the arrays are small.

Usage::

    from cs2_vision_access.adapters.models.runtime.cuda import nms

    keep_indices = nms(boxes, scores, iou_threshold=0.5)
    # keep_indices is a list of int indices into the original arrays
"""

from __future__ import annotations

import numpy as np

# ---------------------------------------------------------------------------
# CPU fallback
# ---------------------------------------------------------------------------


def _nms_cpu(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float = 0.5,
    class_ids: np.ndarray | None = None,
) -> list[int]:
    """Non-maximum suppression on CPU (NumPy).

    Greedy: boxes are visited by descending score and each kept box
    suppresses the later boxes that overlap it above ``iou_threshold``.
    """
    if len(boxes) == 0:
        return []
    if class_ids is not None:
        # Class-aware: a head box must not suppress a body box of another class.
        class_keep: list[int] = []
        for class_id in np.unique(class_ids):
            index = np.flatnonzero(class_ids == class_id)
            class_keep.extend(
                index[j] for j in _nms_cpu(boxes[index], scores[index], iou_threshold)
            )
        return sorted(class_keep, key=lambda i: -float(scores[i]))

    order = np.argsort(-scores)

    x1 = boxes[:, 0]
    y1 = boxes[:, 1]
    x2 = boxes[:, 2]
    y2 = boxes[:, 3]

    areas = (x2 - x1).clip(0) * (y2 - y1).clip(0)
    keep: list[int] = []

    remaining = order.tolist()
    while remaining:
        i = remaining.pop(0)
        keep.append(i)

        xx1 = np.maximum(x1[i], x1[remaining])
        yy1 = np.maximum(y1[i], y1[remaining])
        xx2 = np.minimum(x2[i], x2[remaining])
        yy2 = np.minimum(y2[i], y2[remaining])

        inter = (xx2 - xx1).clip(0) * (yy2 - yy1).clip(0)
        denom = np.maximum(areas[i] + areas[remaining] - inter, 1e-12)
        ovr = inter / denom
        remaining = [remaining[j] for j in range(len(remaining)) if ovr[j] <= iou_threshold]

    return keep


# ---------------------------------------------------------------------------
# Public API, auto-dispatch
# ---------------------------------------------------------------------------


def nms(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float = 0.5,
    class_ids: np.ndarray | None = None,
) -> list[int]:
    """Run Non-Maximum Suppression on bounding boxes.

    CuPy inputs are copied to the host once before suppression.

    Args:
        boxes: ``[N, 4]`` array of ``(x1, y1, x2, y2)`` boxes.
        scores: ``[N]`` array of confidence scores (higher is better).
        iou_threshold: Boxes with IoU above this threshold are suppressed.
        class_ids: Optional ``[N]`` class labels; suppression is per class.

    Returns:
        List of kept box indices (sorted by descending score).
    """
    if hasattr(boxes, "__cuda_array_interface__"):
        import cupy as cp

        boxes = cp.asnumpy(boxes)
        scores = cp.asnumpy(scores)
    return _nms_cpu(boxes, scores, iou_threshold, class_ids)


__all__ = ["nms"]
