"""Non-Maximum Suppression — GPU-accelerated with automatic CPU fallback.

The CUDA kernel sorts boxes by confidence score, then runs a parallel
suppression check: each thread handles one box and checks IoU against
all higher-scoring boxes.  This is significantly faster than the CPU
sequential loop when there are many detections (50+ boxes per frame).

Usage::

    from cs2_vision_access.adapters.models.runtime.cuda import nms

    keep_indices = nms(boxes, scores, iou_threshold=0.5)
    # keep_indices is a list of int indices into the original arrays
"""

from __future__ import annotations

import numpy as np

from cs2_vision_access.adapters.models.runtime.cuda.kernels import NMS_KERNEL_CU
from cs2_vision_access.adapters.models.runtime.cuda.utils import cuda_available

# ---------------------------------------------------------------------------
# CPU fallback
# ---------------------------------------------------------------------------


def _nms_cpu(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float = 0.5,
) -> list[int]:
    """Non-maximum suppression on CPU (NumPy).

    This is the reference implementation — identical logic to the CUDA
    kernel, but runs on CPU.
    """
    if len(boxes) == 0:
        return []

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
        ovr = inter / (areas[i] + areas[remaining] - inter)
        remaining = [remaining[j] for j in range(len(remaining)) if ovr[j] <= iou_threshold]

    return keep


# ---------------------------------------------------------------------------
# GPU (CuPy RawKernel)
# ---------------------------------------------------------------------------

_NMS_GPU_BLOCK = 256


def _nms_gpu(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float = 0.5,
) -> list[int]:
    """Non-maximum suppression on GPU via a CUDA RawKernel.

    The kernel operates on score-sorted boxes for efficient suppression.
    """
    import cupy as cp

    N = len(boxes)
    if N == 0:
        return []

    # Sort by score descending
    order = cp.argsort(-cp.asarray(scores))
    sorted_boxes = cp.asarray(boxes, dtype=cp.float32)[order]
    suppressed = cp.zeros(N, dtype=cp.bool_)

    # Launch kernel
    blocks = (N + _NMS_GPU_BLOCK - 1) // _NMS_GPU_BLOCK
    kernel = cp.RawKernel(NMS_KERNEL_CU, "nms_kernel")
    kernel(
        (blocks,),
        (_NMS_GPU_BLOCK,),
        (sorted_boxes, cp.arange(N, dtype=cp.int32), suppressed, N, cp.float32(iou_threshold)),
    )

    # Gather kept indices (unsorted back to original order via order array)
    suppressed_cpu = cp.asnumpy(suppressed)
    keep_sorted = [int(order[i]) for i in range(N) if not suppressed_cpu[i]]
    return keep_sorted


# ---------------------------------------------------------------------------
# Public API — auto-dispatch
# ---------------------------------------------------------------------------


def nms(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float = 0.5,
) -> list[int]:
    """Run Non-Maximum Suppression on bounding boxes.

    Automatically dispatches to the CUDA kernel when a GPU is available
    and falls back to CPU NumPy otherwise.

    Args:
        boxes: ``[N, 4]`` array of ``(x1, y1, x2, y2)`` boxes.
        scores: ``[N]`` array of confidence scores (higher is better).
        iou_threshold: Boxes with IoU above this threshold are suppressed.

    Returns:
        List of kept box indices (sorted by descending score).
    """
    if cuda_available():
        try:
            return _nms_gpu(boxes, scores, iou_threshold)
        except (ImportError, RuntimeError):
            # A missing CUDA backend or failed kernel falls through to NumPy.
            pass
    return _nms_cpu(boxes, scores, iou_threshold)


__all__ = ["nms"]
