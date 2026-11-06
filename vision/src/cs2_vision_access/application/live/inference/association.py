"""Shared consecutive-frame association heuristics (no persistent track IDs).

Used by both the temporal stability filter (:mod:`cs2_vision_access.
application.live.inference.temporal`) and the track smoother
(:mod:`cs2_vision_access.application.live.inference.tracks`). Keeping the
geometry/association primitives in one place guarantees both consumers
associate detections with the same rule set.

Association is a consecutive-frame heuristic: same class only, candidate when
bbox IoU >= threshold **or** centroid distance <= ``centroid_match_px``;
greedy one-to-one assignment preferring higher IoU, then closer centroid.
"""

from __future__ import annotations

import math
from collections.abc import Sequence

# Association heuristics (no track IDs; consecutive-frame only).
DEFAULT_IOU_THRESHOLD = 0.1
DEFAULT_CENTROID_MATCH_PX = 64.0

Bbox = tuple[float, float, float, float]
Centroid = tuple[float, float]


def centroid(polygon: Sequence[tuple[float, float]]) -> Centroid:
    """Mean of the polygon vertices."""
    total_x = 0.0
    total_y = 0.0
    for x, y in polygon:
        total_x += float(x)
        total_y += float(y)
    count = float(len(polygon))
    return total_x / count, total_y / count


def bbox(polygon: Sequence[tuple[float, float]]) -> Bbox:
    """Axis-aligned bounding box ``(x0, y0, x1, y1)`` of the polygon."""
    xs = [float(x) for x, _ in polygon]
    ys = [float(y) for _, y in polygon]
    return min(xs), min(ys), max(xs), max(ys)


def bbox_iou(first: Bbox, second: Bbox) -> float:
    """Intersection-over-union of two ``(x0, y0, x1, y1)`` boxes."""
    ax0, ay0, ax1, ay1 = first
    bx0, by0, bx1, by1 = second
    ix0 = max(ax0, bx0)
    iy0 = max(ay0, by0)
    ix1 = min(ax1, bx1)
    iy1 = min(ay1, by1)
    iw = max(0.0, ix1 - ix0)
    ih = max(0.0, iy1 - iy0)
    inter = iw * ih
    if inter <= 0.0:
        return 0.0
    area_a = max(0.0, ax1 - ax0) * max(0.0, ay1 - ay0)
    area_b = max(0.0, bx1 - bx0) * max(0.0, by1 - by0)
    union = area_a + area_b - inter
    if union <= 0.0:
        return 0.0
    return inter / union


def associate_greedy(
    previous: Sequence[tuple[int, Centroid, Bbox]],
    current: Sequence[tuple[int, Centroid, Bbox]],
    *,
    iou_threshold: float,
    centroid_match_px: float,
) -> dict[int, int]:
    """Greedy one-to-one match: current index → previous index.

    Same class only. A pair is a candidate when bbox IoU ≥ threshold **or**
    centroid distance ≤ ``centroid_match_px``. Prefer higher IoU, then closer
    centroid.
    """
    candidates: list[tuple[float, float, int, int]] = []
    for cur_index, (class_id, cur_centroid, cur_bbox) in enumerate(current):
        for prev_index, (prev_class_id, prev_centroid, prev_bbox) in enumerate(previous):
            if prev_class_id != class_id:
                continue
            iou = bbox_iou(prev_bbox, cur_bbox)
            distance = math.hypot(
                cur_centroid[0] - prev_centroid[0],
                cur_centroid[1] - prev_centroid[1],
            )
            if iou >= iou_threshold or distance <= centroid_match_px:
                # Sort key: higher IoU first, then smaller distance.
                candidates.append((-iou, distance, cur_index, prev_index))
    candidates.sort()
    matched_cur: set[int] = set()
    matched_prev: set[int] = set()
    mapping: dict[int, int] = {}
    for _neg_iou, _distance, cur_index, prev_index in candidates:
        if cur_index in matched_cur or prev_index in matched_prev:
            continue
        matched_cur.add(cur_index)
        matched_prev.add(prev_index)
        mapping[cur_index] = prev_index
    return mapping
