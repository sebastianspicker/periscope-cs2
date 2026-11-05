"""OpenCV distance-transform outline banding."""

from __future__ import annotations

from typing import Any

import numpy as np

from cs2_vision_access.adapters.rendering.renderer.roi import contour_roi, shift_contours
from cs2_vision_access.domain.outline import DRAW_COORDINATE_SHIFT


def _draw_distance_outline(
    cv2: Any,
    output: np.ndarray,
    fixed_contour: np.ndarray,
    outer_bgr: tuple[int, int, int],
    inner_bgr: tuple[int, int, int],
    outer_width: int,
    inner_width: int,
) -> None:
    """Dual-ish thick outline via distance transform bands (static, deterministic).

    Rasterizes the instance mask, computes exterior/interior distance to the
    boundary, and paints outer then inner color bands near the edge. Same inputs
    always yield the same pixels (no temporal state).
    """
    outer_radius = max(0.5, float(outer_width) / 2.0)
    inner_radius = max(0.5, float(inner_width) / 2.0)
    # Bands never reach past the larger radius, so work on the bbox plus that margin.
    height, width = output.shape[:2]
    margin = int(np.ceil(max(outer_radius, inner_radius))) + 2
    x0, y0, x1, y1 = contour_roi([fixed_contour], margin, height, width)
    if x1 <= x0 or y1 <= y0:
        return
    output = output[y0:y1, x0:x1]
    mask = np.zeros(output.shape[:2], dtype=np.uint8)
    cv2.fillPoly(
        mask,
        shift_contours([fixed_contour], x0, y0),
        255,
        shift=DRAW_COORDINATE_SHIFT,
    )
    # Exterior distance: distance into the background from the filled silhouette.
    inverted = cv2.bitwise_not(mask)
    dist_out = cv2.distanceTransform(inverted, cv2.DIST_L2, 5)
    # Interior distance: distance into the silhouette from the boundary.
    dist_in = cv2.distanceTransform(mask, cv2.DIST_L2, 5)
    edge_dist = np.where(mask > 0, dist_in, dist_out)
    outer_band = edge_dist <= outer_radius
    inner_band = edge_dist <= inner_radius
    output[outer_band] = outer_bgr
    output[inner_band] = inner_bgr
