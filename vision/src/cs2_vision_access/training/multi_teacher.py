"""Multi-teacher consensus and offline temporal consistency helpers.

Pure geometry utilities for pseudo-label fusion:

* :func:`consensus_boxes` — keep detections agreed on by multiple teachers
  via axis-aligned box IoU matching.
* :func:`filter_temporally_unstable` — drop instance tracks that do not
  persist across consecutive frames for ``min_persist`` frames.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.predictions import InstanceMask, Point

# ---------------------------------------------------------------------------
# Box geometry
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Box:
    """Axis-aligned box in absolute pixel coordinates (inclusive-exclusive style xyxy)."""

    x1: float
    y1: float
    x2: float
    y2: float
    class_id: int = 0
    confidence: float = 1.0

    def __post_init__(self) -> None:
        if not all(math.isfinite(v) for v in (self.x1, self.y1, self.x2, self.y2, self.confidence)):
            raise ValueError("box coordinates and confidence must be finite")
        if self.x2 < self.x1 or self.y2 < self.y1:
            raise ValueError("box must satisfy x2 >= x1 and y2 >= y1")
        if self.class_id < 0:
            raise ValueError("class_id must be non-negative")
        if not 0.0 <= self.confidence <= 1.0:
            raise ValueError("confidence must be in [0, 1]")

    @property
    def xyxy(self) -> tuple[float, float, float, float]:
        return (self.x1, self.y1, self.x2, self.y2)

    def area(self) -> float:
        return max(0.0, self.x2 - self.x1) * max(0.0, self.y2 - self.y1)


def box_from_polygon(
    polygon: Sequence[Point],
    *,
    class_id: int = 0,
    confidence: float = 1.0,
) -> Box:
    """Axis-aligned bounding box of a polygon."""
    if len(polygon) < 1:
        raise ValueError("polygon must contain at least one point")
    xs = [float(x) for x, _ in polygon]
    ys = [float(y) for _, y in polygon]
    return Box(
        x1=min(xs),
        y1=min(ys),
        x2=max(xs),
        y2=max(ys),
        class_id=class_id,
        confidence=confidence,
    )


def box_iou(
    a: Box | tuple[float, float, float, float], b: Box | tuple[float, float, float, float]
) -> float:
    """Intersection-over-union of two axis-aligned boxes.

    Accepts :class:`Box` or bare ``(x1, y1, x2, y2)`` tuples. Empty-empty and
    fully disjoint pairs return ``0.0``.
    """
    ax1, ay1, ax2, ay2 = a.xyxy if isinstance(a, Box) else a
    bx1, by1, bx2, by2 = b.xyxy if isinstance(b, Box) else b
    ix1 = max(ax1, bx1)
    iy1 = max(ay1, by1)
    ix2 = min(ax2, bx2)
    iy2 = min(ay2, by2)
    iw = max(0.0, ix2 - ix1)
    ih = max(0.0, iy2 - iy1)
    inter = iw * ih
    if inter <= 0.0:
        return 0.0
    area_a = max(0.0, ax2 - ax1) * max(0.0, ay2 - ay1)
    area_b = max(0.0, bx2 - bx1) * max(0.0, by2 - by1)
    union = area_a + area_b - inter
    if union <= 0.0:
        return 0.0
    return inter / union


def _match_boxes_greedy(
    left: Sequence[Box],
    right: Sequence[Box],
    *,
    iou_thresh: float,
    same_class: bool = True,
) -> list[tuple[int, int, float]]:
    """Greedy one-to-one matches by descending IoU.

    Returns ``(left_index, right_index, iou)`` triples with IoU ≥ ``iou_thresh``.
    """
    candidates: list[tuple[float, int, int]] = []
    for li, lb in enumerate(left):
        for ri, rb in enumerate(right):
            if same_class and lb.class_id != rb.class_id:
                continue
            iou = box_iou(lb, rb)
            if iou >= iou_thresh:
                candidates.append((iou, li, ri))
    candidates.sort(key=lambda item: (-item[0], item[1], item[2]))
    used_l: set[int] = set()
    used_r: set[int] = set()
    matches: list[tuple[int, int, float]] = []
    for iou, li, ri in candidates:
        if li in used_l or ri in used_r:
            continue
        used_l.add(li)
        used_r.add(ri)
        matches.append((li, ri, iou))
    return matches


def consensus_boxes(
    detections_per_teacher: Sequence[Sequence[Box]],
    iou_thresh: float = 0.5,
    *,
    min_votes: int | None = None,
) -> list[Box]:
    """Fuse multi-teacher detections by IoU consensus.

    A detection cluster is kept when at least ``min_votes`` teachers contribute
    a same-class box with IoU ≥ ``iou_thresh``. Default ``min_votes`` is a
    majority: ``ceil(n_teachers / 2)`` (minimum 1 when only one teacher is
    provided). Cluster geometry is the mean xyxy of members; confidence is the
    mean of member confidences; class_id is the mode of member classes
    (first-seen on ties).

    Parameters
    ----------
    detections_per_teacher:
        Outer sequence = one entry per teacher; inner sequence = that teacher's
        boxes for a single frame.
    iou_thresh:
        Minimum IoU for two boxes to be considered the same object.
    min_votes:
        Override the majority threshold. Must be ≥ 1 when set.
    """
    if not 0.0 <= float(iou_thresh) <= 1.0:
        raise ValueError("iou_thresh must be in [0, 1]")
    teachers = [list(dets) for dets in detections_per_teacher]
    n_teachers = len(teachers)
    if n_teachers == 0:
        return []

    if min_votes is None:
        required = max(1, (n_teachers + 1) // 2)
    else:
        if isinstance(min_votes, bool) or not isinstance(min_votes, int) or min_votes < 1:
            raise ValueError("min_votes must be an integer >= 1")
        required = min_votes

    # Seed clusters from every detection; merge across teachers by IoU.
    # Representation: list of (teacher_index, box) members.
    clusters: list[list[tuple[int, Box]]] = []
    for t_idx, dets in enumerate(teachers):
        for box in dets:
            placed = False
            for cluster in clusters:
                # Compare against any member from a *different* teacher.
                for member_t, member_box in cluster:
                    if member_t == t_idx:
                        continue
                    if member_box.class_id != box.class_id:
                        continue
                    if box_iou(member_box, box) >= iou_thresh:
                        # One box per teacher per cluster.
                        if any(mt == t_idx for mt, _ in cluster):
                            # Already has a vote from this teacher; keep higher conf.
                            existing_i = next(i for i, (mt, _) in enumerate(cluster) if mt == t_idx)
                            if box.confidence > cluster[existing_i][1].confidence:
                                cluster[existing_i] = (t_idx, box)
                        else:
                            cluster.append((t_idx, box))
                        placed = True
                        break
                if placed:
                    break
            if not placed:
                clusters.append([(t_idx, box)])

    consensus: list[Box] = []
    for cluster in clusters:
        if len(cluster) < required:
            continue
        boxes = [b for _, b in cluster]
        n = float(len(boxes))
        x1 = sum(b.x1 for b in boxes) / n
        y1 = sum(b.y1 for b in boxes) / n
        x2 = sum(b.x2 for b in boxes) / n
        y2 = sum(b.y2 for b in boxes) / n
        conf = sum(b.confidence for b in boxes) / n
        # Mode class_id (stable: first-seen on ties).
        counts: dict[int, int] = {}
        order: list[int] = []
        for b in boxes:
            if b.class_id not in counts:
                order.append(b.class_id)
                counts[b.class_id] = 0
            counts[b.class_id] += 1
        class_id = max(order, key=lambda c: counts[c])
        consensus.append(Box(x1=x1, y1=y1, x2=x2, y2=y2, class_id=class_id, confidence=conf))
    # Stable order: descending confidence, then class, then x1.
    consensus.sort(key=lambda b: (-b.confidence, b.class_id, b.x1, b.y1))
    return consensus


# ---------------------------------------------------------------------------
# Temporal consistency (offline, batch)
# ---------------------------------------------------------------------------


def _mask_bbox(mask: InstanceMask) -> Box:
    return box_from_polygon(mask.polygon, class_id=mask.class_id, confidence=mask.confidence)


def filter_temporally_unstable(
    frame_labels: Sequence[Sequence[InstanceMask]],
    min_persist: int = 2,
    *,
    iou_thresh: float = 0.3,
) -> list[list[InstanceMask]]:
    """Drop instances that do not persist across consecutive frames.

    Uses simple greedy bbox IoU matching between consecutive frames (same
    class only). An instance is kept on a frame if its track length across
    the sequence is ≥ ``min_persist``. Tracks of length 1 (single-frame
    blips) are dropped when ``min_persist >= 2``.

    Parameters
    ----------
    frame_labels:
        Ordered list of per-frame instance lists (frame 0, 1, …).
    min_persist:
        Minimum number of consecutive-frame appearances required to keep
        an instance. Defaults to 2.
    iou_thresh:
        Minimum bbox IoU for consecutive-frame association.

    Returns
    -------
    list[list[InstanceMask]]
        Filtered labels with the same outer length as ``frame_labels``.
    """
    if isinstance(min_persist, bool) or not isinstance(min_persist, int) or min_persist < 1:
        raise ValueError("min_persist must be an integer >= 1")
    if not 0.0 <= float(iou_thresh) <= 1.0:
        raise ValueError("iou_thresh must be in [0, 1]")

    frames: list[list[InstanceMask]] = [list(frame) for frame in frame_labels]
    n_frames = len(frames)
    if n_frames == 0:
        return []
    if min_persist <= 1:
        # Nothing to filter — every appearance is already long enough.
        return frames

    # track_id assignment: each (frame_i, instance_j) → track_id
    # Build tracks by chaining consecutive-frame matches.
    next_track_id = 0
    # assignment[frame_idx][inst_idx] = track_id
    assignment: list[list[int]] = []
    track_lengths: dict[int, int] = {}

    for frame_idx, instances in enumerate(frames):
        frame_assign = [-1] * len(instances)
        if frame_idx == 0:
            for j in range(len(instances)):
                tid = next_track_id
                next_track_id += 1
                frame_assign[j] = tid
                track_lengths[tid] = 1
            assignment.append(frame_assign)
            continue

        prev_instances = frames[frame_idx - 1]
        prev_assign = assignment[frame_idx - 1]
        prev_boxes = [_mask_bbox(m) for m in prev_instances]
        cur_boxes = [_mask_bbox(m) for m in instances]
        matches = _match_boxes_greedy(prev_boxes, cur_boxes, iou_thresh=iou_thresh)

        matched_cur: set[int] = set()
        for prev_i, cur_i, _iou in matches:
            tid = prev_assign[prev_i]
            frame_assign[cur_i] = tid
            track_lengths[tid] = track_lengths.get(tid, 0) + 1
            matched_cur.add(cur_i)

        for j in range(len(instances)):
            if j in matched_cur:
                continue
            tid = next_track_id
            next_track_id += 1
            frame_assign[j] = tid
            track_lengths[tid] = 1
        assignment.append(frame_assign)

    # Keep only instances whose track length >= min_persist.
    filtered: list[list[InstanceMask]] = []
    for frame_idx, instances in enumerate(frames):
        kept: list[InstanceMask] = []
        for j, mask in enumerate(instances):
            tid = assignment[frame_idx][j]
            if track_lengths.get(tid, 0) >= min_persist:
                kept.append(mask)
        filtered.append(kept)
    return filtered


__all__ = [
    "Box",
    "box_from_polygon",
    "box_iou",
    "consensus_boxes",
    "filter_temporally_unstable",
]
