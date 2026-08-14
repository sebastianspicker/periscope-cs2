"""GPU-accelerated image preprocessing and YOLO output decoding.

Provides CUDA-optimised versions of the most expensive CPU operations in the
inference pipeline:

1. **Letterbox** — BGR uint8 frame → RGB float32 CHW tensor with letterbox
   padding, bilinear interpolation, and [0, 1] normalisation.
2. **YOLO decode** — Raw Ultralytics ONNX output → xyxy boxes + scores + class
   IDs (with confidence filtering).

Both functions fall back to CPU (OpenCV + NumPy) when CUDA is unavailable.
"""

from __future__ import annotations

import numpy as np

from cs2_vision_access.inference.cuda.kernels import (
    LETTERBOX_KERNEL_CU,
    YOLO_DECODE_KERNEL_CU,
)
from cs2_vision_access.inference.cuda.utils import cuda_available

# =========================================================================
# GPU letterbox (BGR → RGB CHW float32)
# =========================================================================


def _letterbox_cpu(
    frame_bgr: np.ndarray,
    target_size: int,
) -> tuple[np.ndarray, float, float, int, int]:
    """Reference CPU implementation using OpenCV."""
    import cv2

    h, w = frame_bgr.shape[:2]
    scale = target_size / max(h, w)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))

    resized = cv2.resize(frame_bgr, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

    pad_left = (target_size - new_w) // 2
    pad_top = (target_size - new_h) // 2
    pad_right = target_size - new_w - pad_left
    pad_bottom = target_size - new_h - pad_top

    padded = cv2.copyMakeBorder(
        resized,
        pad_top,
        pad_bottom,
        pad_left,
        pad_right,
        cv2.BORDER_CONSTANT,
        value=(0, 0, 0),
    )

    rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    chw = np.transpose(rgb, (2, 0, 1))
    batch = np.expand_dims(chw, axis=0)

    actual_scale_x = w / new_w
    actual_scale_y = h / new_h

    return batch, actual_scale_x, actual_scale_y, pad_left, pad_top


def _letterbox_gpu(
    frame_bgr: np.ndarray,
    target_size: int,
) -> tuple[np.ndarray, float, float, int, int]:
    """GPU-accelerated letterbox using a CUDA RawKernel."""
    import cupy as cp

    h, w = frame_bgr.shape[:2]
    scale = target_size / max(h, w)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))
    pad_left = (target_size - new_w) // 2
    pad_top = (target_size - new_h) // 2

    inv_scale_x = w / new_w  # src_w / resized_w
    inv_scale_y = h / new_h

    # Upload frame to GPU
    src_gpu = cp.asarray(frame_bgr, dtype=cp.uint8)  # [H, W, 3] BGR
    dst_gpu = cp.zeros((3, target_size, target_size), dtype=cp.float32)

    # 3D grid: (c, y, x)
    threads = (16, 16, 1)
    blocks = (
        (target_size + threads[0] - 1) // threads[0],
        (target_size + threads[1] - 1) // threads[1],
        3,  # one block per channel
    )

    kernel = cp.RawKernel(LETTERBOX_KERNEL_CU, "letterbox_kernel")
    kernel(
        blocks,
        threads,
        (
            src_gpu,
            w,
            h,
            frame_bgr.strides[0],  # stride = row bytes
            dst_gpu,
            target_size,
            target_size,
            cp.float32(inv_scale_x),
            cp.float32(inv_scale_y),
            pad_left,
            pad_top,
        ),
    )

    # Download result
    chw = cp.asnumpy(dst_gpu)  # [3, target_size, target_size]
    batch = np.expand_dims(chw, axis=0)  # [1, 3, T, T]

    return batch, inv_scale_x, inv_scale_y, pad_left, pad_top


def gpu_letterbox(
    frame_bgr: np.ndarray,
    target_size: int,
) -> tuple[np.ndarray, float, float, int, int]:
    """BGR frame → RGB CHW float32 tensor with letterbox padding.

    Auto-dispatches to GPU or CPU.

    Returns:
        ``(tensor, scale_x, scale_y, pad_left, pad_top)``
        - tensor: float32 ``[1, 3, target_size, target_size]``, RGB, [0, 1]
        - scale_x, scale_y: multipliers to map output coords → original pixels
        - pad_left, pad_top: pixels of letterbox padding
    """
    if cuda_available():
        try:
            return _letterbox_gpu(frame_bgr, target_size)
        except Exception:
            pass
    return _letterbox_cpu(frame_bgr, target_size)


# =========================================================================
# GPU YOLO box decode
# =========================================================================


def _yolo_decode_cpu(
    raw: np.ndarray,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
    confidence: float = 0.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Reference CPU YOLO decode using NumPy."""
    if raw.ndim == 3:
        raw = raw[0]

    num_predictions = raw.shape[1]

    # Box params
    cx = raw[0, :] / det_size * orig_w
    cy = raw[1, :] / det_size * orig_h
    w = raw[2, :] / det_size * orig_w
    h = raw[3, :] / det_size * orig_h

    x1 = (cx - w / 2).clip(0, orig_w)
    y1 = (cy - h / 2).clip(0, orig_h)
    x2 = (cx + w / 2).clip(0, orig_w)
    y2 = (cy + h / 2).clip(0, orig_h)

    boxes = np.stack([x1, y1, x2, y2], axis=1).astype(np.float32)

    # Class argmax
    cls_data = raw[4 : 4 + num_classes, :]
    class_ids = cls_data.argmax(axis=0).astype(np.int32)
    scores = cls_data[class_ids, np.arange(num_predictions)]

    # Filter
    mask = scores >= confidence
    return boxes[mask], scores[mask], class_ids[mask]


def _yolo_decode_gpu(
    raw: np.ndarray,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
    confidence: float = 0.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """GPU-accelerated YOLO output decode."""
    import cupy as cp

    num_predictions = raw.shape[1]

    raw_gpu = cp.asarray(raw, dtype=cp.float32)
    boxes_gpu = cp.zeros((num_predictions, 4), dtype=cp.float32)
    scores_gpu = cp.zeros(num_predictions, dtype=cp.float32)
    class_ids_gpu = cp.zeros(num_predictions, dtype=cp.int32)

    threads = 256
    blocks = (num_predictions + threads - 1) // threads

    kernel = cp.RawKernel(YOLO_DECODE_KERNEL_CU, "yolo_decode_kernel")
    kernel(
        (blocks,),
        (threads,),
        (
            raw_gpu,
            num_classes,
            num_predictions,
            cp.float32(det_size),
            cp.float32(orig_w),
            cp.float32(orig_h),
            boxes_gpu,
            scores_gpu,
            class_ids_gpu,
        ),
    )

    # Download
    boxes = cp.asnumpy(boxes_gpu)
    scores = cp.asnumpy(scores_gpu)
    class_ids = cp.asnumpy(class_ids_gpu)

    # Filter
    mask = scores >= confidence
    return boxes[mask], scores[mask], class_ids[mask]


def gpu_yolo_decode(
    raw: np.ndarray,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int = 640,
    confidence: float = 0.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Decode raw Ultralytics ONNX output into filtered boxes.

    Returns:
        ``(boxes, scores, class_ids)`` where
        - boxes: ``[M, 4]`` ``(x1, y1, x2, y2)`` in original pixel space
        - scores: ``[M]`` confidence scores
        - class_ids: ``[M]`` class IDs
        Only predictions with ``score >= confidence`` are returned.
    """
    if cuda_available():
        try:
            return _yolo_decode_gpu(raw, num_classes, orig_w, orig_h, det_size, confidence)
        except Exception:
            pass
    return _yolo_decode_cpu(raw, num_classes, orig_w, orig_h, det_size, confidence)


# =========================================================================
# GPU marching squares contour extraction
# =========================================================================


def gpu_mask_to_polygon(
    mask_logits: np.ndarray,
    epsilon: float = 2.0,
    threshold: float = 0.5,
    max_vertices: int = 8192,
) -> tuple[tuple[float, float], ...] | None:
    """Extract a contour polygon from a mask on GPU.

    Uses the CUDA marching squares kernel followed by CPU simplification.

    Args:
        mask_logits: 2D array (H, W) of mask values. May be either raw logits
            (arbitrary range) or already-binarised probabilities (values in
            [0, 1]). Inputs whose values all lie in ``[0, 1]`` are treated as
            probabilities; otherwise a sigmoid is applied first.
        epsilon: Polyline simplification tolerance (pixels).
        threshold: Foreground cutoff. Interpreted in probability space (e.g.
            ``0.5``) when the input is binary/probabilities, and in logit
            space (e.g. ``0.0``, the SAM logit boundary) when a sigmoid is
            applied.
        max_vertices: Maximum number of contour vertices to emit.

    Returns:
        Tuple of ``(x, y)`` points or ``None`` if CUDA is unavailable.
    """
    if not cuda_available():
        return None

    from cs2_vision_access.inference.cuda.kernels import MARCHING_SQUARES_CU

    try:
        import cupy as cp
    except ImportError:
        raise RuntimeError("CUDA is available but cupy is not installed") from None

    H, W = mask_logits.shape

    # Only apply sigmoid to true logits; already-binarised/probability inputs
    # (all values in [0, 1]) are passed through unchanged.
    vmin = float(mask_logits.min())
    vmax = float(mask_logits.max())
    if vmin >= 0.0 and vmax <= 1.0:
        probs_gpu = cp.asarray(mask_logits, dtype=cp.float32)
    else:
        mask_gpu = cp.asarray(mask_logits, dtype=cp.float32)
        probs_gpu = 1.0 / (1.0 + cp.exp(-mask_gpu))

    # Allocate output buffers
    vertices_gpu = cp.zeros((max_vertices, 2), dtype=cp.float32)
    count_gpu = cp.zeros(1, dtype=cp.int32)

    # Launch kernel
    threads = (16, 16)
    blocks = (
        (W + 15) // 16,
        (H + 15) // 16,
    )

    kernel = cp.RawKernel(MARCHING_SQUARES_CU, "marching_squares_kernel")
    kernel(
        blocks,
        threads,
        (probs_gpu, H, W, cp.float32(threshold), vertices_gpu, count_gpu, max_vertices),
    )

    num_vertices = int(cp.asnumpy(count_gpu)[0])
    if num_vertices < 6:  # fewer than 3 line segments = no useful contour
        return ()

    # Defensive clamping: never read more rows than were allocated, and the
    # vertex stream is made of pairs so the count must stay even.
    num_vertices = min(num_vertices, max_vertices)
    if num_vertices % 2:
        num_vertices -= 1

    vertices = cp.asnumpy(vertices_gpu[:num_vertices])

    # Simplify the raw contour segments using CPU Douglas-Peucker
    import cv2

    # Rasterise the emitted line segments onto a binary image, then let
    # OpenCV's findContours produce an ordered polygon.  The raster is a
    # plain NumPy array (CPU work) so cv2.line / findContours work directly.
    raster = np.zeros((H, W), dtype=np.uint8)
    for i in range(0, num_vertices, 2):
        x1, y1 = int(vertices[i][0]), int(vertices[i][1])
        x2, y2 = int(vertices[i + 1][0]), int(vertices[i + 1][1])
        # Clip to bounds
        x1, y1 = max(0, min(W - 1, x1)), max(0, min(H - 1, y1))
        x2, y2 = max(0, min(W - 1, x2)), max(0, min(H - 1, y2))
        try:
            cv2.line(raster, (x1, y1), (x2, y2), 255, 1)
        except TypeError:  # pragma: no cover - defensive
            raster[y1, x1] = 255
            raster[y2, x2] = 255

    contours, _ = cv2.findContours(raster, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return ()
    largest = max(contours, key=cv2.contourArea)
    if len(largest) < 3:
        return ()
    simplified = cv2.approxPolyDP(largest, epsilon, closed=True)
    return tuple((float(p[0][0]), float(p[0][1])) for p in simplified)


__all__ = ["gpu_letterbox", "gpu_mask_to_polygon", "gpu_yolo_decode"]
