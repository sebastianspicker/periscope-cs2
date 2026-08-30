"""Jump Flood Algorithm (JFA) distance fields and outline bands."""

from __future__ import annotations

from typing import Any

import numpy as np

from cs2_vision_access.domain.outline import DRAW_COORDINATE_SHIFT


def _jfa_distance_to_seeds(seed_mask: np.ndarray) -> np.ndarray:
    """Multi-step Jump Flood distance field to nearest seed (pure numpy).

    Seeds every truthy pixel in ``seed_mask`` with its own ``(x, y)``, then
    propagates nearest-seed coordinates in ``O(log max(H, W))`` jump steps
    (step = N/2, N/4, …, 1). Returns float32 Euclidean distances; unreachable
    pixels get a large finite sentinel. Fully deterministic (no RNG / time).
    """
    seed = np.asarray(seed_mask, dtype=bool)
    height, width = seed.shape
    if height == 0 or width == 0:
        return np.zeros((height, width), dtype=np.float32)

    empty = np.int32(-1)
    seed_x = np.full((height, width), empty, dtype=np.int32)
    seed_y = np.full((height, width), empty, dtype=np.int32)
    yy, xx = np.indices((height, width), dtype=np.int32)
    seed_x[seed] = xx[seed]
    seed_y[seed] = yy[seed]

    large = np.float32(1.0e9)
    if not np.any(seed):
        return np.full((height, width), large, dtype=np.float32)

    max_dim = max(height, width)
    step = 1
    while step < max_dim:
        step *= 2
    step //= 2

    # Squared-distance sentinel: large but safe under int64 multiply.
    max_d2 = np.int64(10**18)

    while step >= 1:
        best_x = seed_x.copy()
        best_y = seed_y.copy()
        best_d2 = np.full((height, width), max_d2, dtype=np.int64)
        valid = seed_x >= 0
        if np.any(valid):
            dx0 = xx[valid].astype(np.int64) - seed_x[valid].astype(np.int64)
            dy0 = yy[valid].astype(np.int64) - seed_y[valid].astype(np.int64)
            best_d2[valid] = dx0 * dx0 + dy0 * dy0

        for offset_y in (-step, 0, step):
            for offset_x in (-step, 0, step):
                if offset_x == 0 and offset_y == 0:
                    continue
                # Neighbor cell's seed → rolled onto this pixel; invalidate wraps.
                neigh_x = np.roll(
                    np.roll(seed_x, shift=-offset_y, axis=0),
                    shift=-offset_x,
                    axis=1,
                )
                neigh_y = np.roll(
                    np.roll(seed_y, shift=-offset_y, axis=0),
                    shift=-offset_x,
                    axis=1,
                )
                if offset_y > 0:
                    neigh_x[:offset_y, :] = empty
                    neigh_y[:offset_y, :] = empty
                elif offset_y < 0:
                    neigh_x[offset_y:, :] = empty
                    neigh_y[offset_y:, :] = empty
                if offset_x > 0:
                    neigh_x[:, :offset_x] = empty
                    neigh_y[:, :offset_x] = empty
                elif offset_x < 0:
                    neigh_x[:, offset_x:] = empty
                    neigh_y[:, offset_x:] = empty

                n_valid = neigh_x >= 0
                if not np.any(n_valid):
                    continue
                cand_d2 = np.full((height, width), max_d2, dtype=np.int64)
                ddx = xx[n_valid].astype(np.int64) - neigh_x[n_valid].astype(np.int64)
                ddy = yy[n_valid].astype(np.int64) - neigh_y[n_valid].astype(np.int64)
                cand_d2[n_valid] = ddx * ddx + ddy * ddy
                # Strict < keeps first seed on ties → stable, deterministic field.
                better = n_valid & (cand_d2 < best_d2)
                best_d2 = np.where(better, cand_d2, best_d2)
                best_x = np.where(better, neigh_x, best_x)
                best_y = np.where(better, neigh_y, best_y)

        seed_x = best_x
        seed_y = best_y
        step //= 2

    dist = np.full((height, width), large, dtype=np.float32)
    valid = seed_x >= 0
    if np.any(valid):
        ddx = (xx[valid] - seed_x[valid]).astype(np.float64)
        ddy = (yy[valid] - seed_y[valid]).astype(np.float64)
        dist[valid] = np.sqrt(ddx * ddx + ddy * ddy).astype(np.float32)
    return dist


def _draw_jfa_outline(
    cv2: Any,
    output: np.ndarray,
    fixed_contour: np.ndarray,
    outer_bgr: tuple[int, int, int],
    inner_bgr: tuple[int, int, int],
    outer_width: int,
    inner_width: int,
) -> None:
    """Dual-stroke thick outline via multi-step Jump Flood distance bands.

    Rasterizes the instance mask, seeds filled pixels (exterior field) and
    empty pixels (interior field), runs pure-numpy JFA for each, then paints
    outer then inner color bands near the edge — same band model as the
    ``distance`` kernel, but the field is Jump Flood rather than
    ``cv2.distanceTransform``. Static: same inputs → same pixels.
    """
    height, width = output.shape[:2]
    mask = np.zeros((height, width), dtype=np.uint8)
    cv2.fillPoly(
        mask,
        [fixed_contour],
        255,
        shift=DRAW_COORDINATE_SHIFT,
    )
    filled = mask > 0
    # Exterior: distance to nearest filled seed. Interior: to nearest empty seed.
    dist_out = _jfa_distance_to_seeds(filled)
    dist_in = _jfa_distance_to_seeds(~filled)
    edge_dist = np.where(filled, dist_in, dist_out)
    outer_radius = max(0.5, float(outer_width) / 2.0)
    inner_radius = max(0.5, float(inner_width) / 2.0)
    outer_band = edge_dist <= outer_radius
    inner_band = edge_dist <= inner_radius
    output[outer_band] = outer_bgr
    output[inner_band] = inner_bgr
