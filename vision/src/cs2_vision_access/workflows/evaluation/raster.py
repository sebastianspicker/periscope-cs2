"""Pure-numpy polygon rasterization and mask morphology helpers.

Used by geometry metrics so unit tests and runtime share one definition of
mask IoU / boundary F1 (OpenCV is not consulted here).
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from typing import cast

import numpy as np

from cs2_vision_access.domain.predictions import Point
from cs2_vision_access.workflows.evaluation.errors import EvaluationError


def rasterize_polygon(
    polygon: Sequence[Point],
    *,
    height: int,
    width: int,
) -> np.ndarray:
    """Rasterize a polygon to a boolean mask of shape ``(height, width)``.

    Uses the pure-numpy fill exclusively so matching thresholds are stable
    across environments with or without OpenCV installed.
    """
    if height <= 0 or width <= 0:
        raise EvaluationError("height and width must be positive")
    if len(polygon) < 3:
        raise EvaluationError("polygon must contain at least three points")
    points = np.asarray(polygon, dtype=np.float64)
    if points.ndim != 2 or points.shape[1] != 2:
        raise EvaluationError("polygon must be a sequence of (x, y) pairs")
    if not np.all(np.isfinite(points)):
        raise EvaluationError("polygon coordinates must be finite")
    return np.asarray(_rasterize_polygon_numpy(points, height=height, width=width), dtype=bool)


def mask_boundary(mask: np.ndarray) -> np.ndarray:
    """Pixels in ``mask`` that have a non-mask 4-neighbor (or image edge)."""
    if mask.dtype != bool:
        mask = mask.astype(bool)
    if not mask.any():
        return np.zeros_like(mask, dtype=bool)
    padded = np.pad(mask, 1, mode="constant", constant_values=False)
    center = padded[1:-1, 1:-1]
    up = padded[:-2, 1:-1]
    down = padded[2:, 1:-1]
    left = padded[1:-1, :-2]
    right = padded[1:-1, 2:]
    interior = center & up & down & left & right
    return cast(np.ndarray, center & ~interior)


def dilate(mask: np.ndarray, radius: int) -> np.ndarray:
    """Square dilation via pure numpy (authoritative path; no OpenCV)."""
    if radius <= 0:
        return mask.astype(bool)
    return _dilate_mask_numpy(mask, radius)


def points_in_polygon(xs: np.ndarray, ys: np.ndarray, polygon: np.ndarray) -> np.ndarray:
    """Ray-casting point-in-polygon for arrays of sample points."""
    count = len(xs)
    inside = np.zeros(count, dtype=bool)
    x_poly = polygon[:, 0]
    y_poly = polygon[:, 1]
    n = len(polygon)
    j = n - 1
    for i in range(n):
        xi = x_poly[i]
        yi = y_poly[i]
        xj = x_poly[j]
        yj = y_poly[j]
        intersects = ((yi > ys) != (yj > ys)) & (
            xs < (xj - xi) * (ys - yi) / (yj - yi + np.finfo(float).eps) + xi
        )
        inside ^= intersects
        j = i
    return inside


# Private aliases kept for call sites that used the pre-split names.
_mask_boundary = mask_boundary
_dilate_mask = dilate
_points_in_polygon = points_in_polygon


def _dilate_mask_numpy(mask: np.ndarray, radius: int) -> np.ndarray:
    height, width = mask.shape
    dilated = np.zeros_like(mask, dtype=bool)
    rows, cols = np.nonzero(mask)
    for row, col in zip(rows.tolist(), cols.tolist(), strict=False):
        r0 = max(0, row - radius)
        r1 = min(height, row + radius + 1)
        c0 = max(0, col - radius)
        c1 = min(width, col + radius + 1)
        dilated[r0:r1, c0:c1] = True
    return dilated


def _rasterize_polygon_numpy(points: np.ndarray, *, height: int, width: int) -> np.ndarray:
    """Axis-aligned bounding-box point-in-polygon fill (authoritative path)."""
    mask = np.zeros((height, width), dtype=bool)
    min_x = max(0, int(math.floor(float(points[:, 0].min()))))
    max_x = min(width - 1, int(math.ceil(float(points[:, 0].max()))))
    min_y = max(0, int(math.floor(float(points[:, 1].min()))))
    max_y = min(height - 1, int(math.ceil(float(points[:, 1].max()))))
    if min_x > max_x or min_y > max_y:
        return mask
    xs = np.arange(min_x, max_x + 1, dtype=np.float64)
    ys = np.arange(min_y, max_y + 1, dtype=np.float64)
    grid_x, grid_y = np.meshgrid(xs, ys)
    inside = points_in_polygon(grid_x.ravel(), grid_y.ravel(), points)
    mask[min_y : max_y + 1, min_x : max_x + 1] = inside.reshape(
        (max_y - min_y + 1, max_x - min_x + 1)
    )
    return mask
