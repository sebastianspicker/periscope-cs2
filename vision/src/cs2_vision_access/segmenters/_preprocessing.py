"""Preprocessing utilities for the CS2-SAM hybrid segmenter.

Separated from ``cs2_sam.py`` to reduce the monolith and allow reuse across
backends.
"""

from __future__ import annotations

import numpy as np

# EdgeSAM image encoder settings.
SAM_IMAGE_SIZE = 1024
SAM_MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
SAM_STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)

# Vombit detection input size.
DETECTION_SIZE = 640


def preprocess_vombit(frame_bgr: np.ndarray) -> np.ndarray:
    """Preprocess a BGR frame for Vombit YOLOv10 detection.

    The exported ONNX expects:
    - BGR, 0–255, uint8 or float32
    - Resized to 640×640 (stretch, the Ultralytics-export model handles this)

    Returns:
        float32 tensor ``[1, 3, 640, 640]``.
    """
    import cv2

    resized = cv2.resize(
        frame_bgr, (DETECTION_SIZE, DETECTION_SIZE), interpolation=cv2.INTER_LINEAR
    )
    chw = np.transpose(resized.astype(np.float32), (2, 0, 1))
    return np.expand_dims(chw, axis=0)


def preprocess_sam(frame_bgr: np.ndarray) -> np.ndarray:
    """Preprocess a BGR frame for the EdgeSAM image encoder.

    EdgeSAM expects:
    - RGB, float32, 1024×1024
    - ImageNet normalisation (mean, std)

    Returns:
        float32 tensor ``[1, 3, 1024, 1024]``.
    """
    import cv2

    # BGR → RGB, resize.
    rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
    resized = cv2.resize(rgb, (SAM_IMAGE_SIZE, SAM_IMAGE_SIZE), interpolation=cv2.INTER_LINEAR)

    # Normalise and CHW.
    img = resized.astype(np.float32) / 255.0
    img = (img - SAM_MEAN) / SAM_STD
    chw = np.transpose(img, (2, 0, 1))
    return np.expand_dims(chw, axis=0)


def box_to_sam_prompt(
    box: tuple[float, float, float, float],
    original_size: tuple[int, int],
    sam_size: int,
) -> tuple[np.ndarray, np.ndarray]:
    """Convert a frame-pixel bounding box to EdgeSAM point prompts.

    SAM encodes a box as two corner points with labels:
        label 2 = top-left corner
        label 3 = bottom-right corner

    Coordinates are in the SAM preprocessed (1024×1024) space.
    """
    orig_h, orig_w = original_size
    x1, y1, x2, y2 = box

    scale_x = sam_size / orig_w
    scale_y = sam_size / orig_h

    coords = np.array(
        [
            [x1 * scale_x, y1 * scale_y],
            [x2 * scale_x, y2 * scale_y],
        ],
        dtype=np.float32,
    ).reshape(1, 2, 2)

    labels = np.array([[2, 3]], dtype=np.float32)
    return coords, labels


def _mask_to_polygon_cpu(
    mask_binary: np.ndarray,
    epsilon: float,
    width: int | None,
    height: int | None,
) -> tuple[tuple[float, float], ...] | None:
    """CPU implementation of mask polygon extraction."""
    import cv2

    if (
        width is not None
        and height is not None
        and (mask_binary.shape[0] != height or mask_binary.shape[1] != width)
    ):
        mask_binary = cv2.resize(mask_binary, (width, height), interpolation=cv2.INTER_NEAREST)

    contours, _ = cv2.findContours(mask_binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return ()
    contour = max(contours, key=cv2.contourArea)
    if len(contour) < 3:
        return ()
    points_np = np.asarray(contour, dtype=np.float32).reshape(-1, 2)
    if points_np.ndim != 2 or points_np.shape[1] != 2 or len(points_np) < 3:
        return ()
    if not np.all(np.isfinite(points_np)):
        raise ValueError("mask polygon contains non-finite coordinates")
    if width is not None and height is not None:
        if np.any(
            (points_np[:, 0] < -1.0)
            | (points_np[:, 0] > float(width) + 1.0)
            | (points_np[:, 1] < -1.0)
            | (points_np[:, 1] > float(height) + 1.0)
        ):
            raise ValueError("mask polygon is outside the source frame")
        points_np[:, 0] = np.clip(points_np[:, 0], 0.0, float(width))
        points_np[:, 1] = np.clip(points_np[:, 1], 0.0, float(height))

    simplified = cv2.approxPolyDP(points_np, epsilon, closed=True)
    return tuple((float(p[0][0]), float(p[0][1])) for p in simplified)


def mask_to_polygon(
    mask: np.ndarray,
    epsilon: float = 2.0,
    *,
    width: int | None = None,
    height: int | None = None,
) -> tuple[tuple[float, float], ...] | None:
    """Convert a binary mask to a simplified polygon contour.

    Uses the CUDA marching squares kernel when available, falling back to
    CPU OpenCV ``findContours``.

    Args:
        mask: 2D binary array (H, W) — values > 0 are foreground.
        epsilon: Polyline simplification tolerance (pixels).
        width: Optional target width — mask is resized if different.
        height: Optional target height — mask is resized if different.

    Returns:
        Tuple of (x, y) points forming the outer contour, or ``None`` if no
        valid contour is found. Returns empty tuple ``()`` for no contour
        (legacy callers expect this).
    """
    if mask.ndim != 2:
        raise ValueError("mask must be 2D")

    # GPU path (marching squares).
    try:
        from cs2_vision_access.inference.cuda.utils import cuda_available

        if cuda_available():
            from cs2_vision_access.inference.cuda.preprocess import gpu_mask_to_polygon

            result = gpu_mask_to_polygon(mask.astype(np.float32), epsilon=epsilon)
            if result is not None and result:
                return result
    except Exception:
        pass  # Fall through to CPU

    # CPU fallback.
    binary = (mask > 0).astype(np.uint8) * 255
    return _mask_to_polygon_cpu(binary, epsilon, width, height)
