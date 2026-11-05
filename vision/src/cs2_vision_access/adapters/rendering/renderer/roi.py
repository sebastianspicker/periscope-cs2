"""Bounding-box helpers so per-instance raster work skips the full frame."""

from __future__ import annotations

import numpy as np

from cs2_vision_access.domain.outline import DRAW_COORDINATE_SCALE, DRAW_COORDINATE_SHIFT


def contour_roi(
    contours: list[np.ndarray], margin: int, height: int, width: int
) -> tuple[int, int, int, int]:
    """Pixel box ``(x0, y0, x1, y1)`` around fixed-point contours plus margin, frame-clipped."""
    points = np.concatenate([np.asarray(c).reshape(-1, 2) for c in contours])
    x0, y0 = (points.min(axis=0) >> DRAW_COORDINATE_SHIFT) - margin
    x1, y1 = (points.max(axis=0) >> DRAW_COORDINATE_SHIFT) + margin + 1
    x0, y0 = max(0, int(x0)), max(0, int(y0))
    x1, y1 = min(width, int(x1)), min(height, int(y1))
    return x0, y0, max(x0, x1), max(y0, y1)


def shift_contours(contours: list[np.ndarray], x0: int, y0: int) -> list[np.ndarray]:
    """Translate fixed-point contours into ROI-local coordinates."""
    offset = np.array([x0, y0], dtype=np.int32) * DRAW_COORDINATE_SCALE
    return [(np.asarray(c).astype(np.int32) - offset).reshape(np.shape(c)) for c in contours]
