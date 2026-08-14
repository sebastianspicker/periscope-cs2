"""Geometric dash/dot segmentation along fixed-point contours."""

from __future__ import annotations

import numpy as np

from cs2_vision_access.renderer.style import DRAW_COORDINATE_SCALE


def pattern_polyline_segments(
    contour_fixed: np.ndarray,
    pattern: str,
    period_px: int,
) -> list[np.ndarray]:
    """Split a closed fixed-point contour into open dash/dot segments.

    Segmentation is purely geometric along arc length. There is no phase offset
    or temporal state: the same contour always yields the same segments.
    """
    if pattern not in {"dashed", "dotted"}:
        raise ValueError(f"pattern_polyline_segments requires dashed or dotted, got {pattern!r}")
    points_px = contour_fixed.reshape((-1, 2)).astype(np.float64) / DRAW_COORDINATE_SCALE
    if points_px.shape[0] < 2:
        return []

    path = np.vstack([points_px, points_px[:1]])
    period = max(1.0, float(period_px))
    on_len = period * 0.5 if pattern == "dashed" else max(1.0, period * 0.25)
    cycle = period
    if on_len >= cycle:
        # Degenerate period: draw the closed contour as a single solid-like loop
        # via one open polyline that covers the whole perimeter.
        full = np.rint(path * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
        return [full]

    segments: list[np.ndarray] = []
    current: list[np.ndarray] = []
    dist = 0.0

    def flush() -> None:
        nonlocal current
        if len(current) >= 2:
            stacked = np.stack(current, axis=0)
            fixed = np.rint(stacked * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
            segments.append(fixed)
        current = []

    for index in range(len(path) - 1):
        start = path[index]
        end = path[index + 1]
        edge = end - start
        edge_len = float(np.linalg.norm(edge))
        if edge_len <= 1e-9:
            continue
        unit = edge / edge_len
        local = 0.0
        remaining = edge_len
        while remaining > 1e-9:
            phase = dist % cycle
            if phase < on_len - 1e-12:
                run = min(on_len - phase, remaining)
                start_pt = start + unit * local
                end_pt = start + unit * (local + run)
                if not current:
                    current.append(start_pt.copy())
                # Avoid duplicating the previous end when continuing an on-run
                # across edges; only append the new endpoint.
                current.append(end_pt.copy())
                local += run
                dist += run
                remaining -= run
                if (phase + run) >= on_len - 1e-12:
                    flush()
            else:
                run = min(cycle - phase, remaining)
                local += run
                dist += run
                remaining -= run
                flush()

    flush()
    return segments
