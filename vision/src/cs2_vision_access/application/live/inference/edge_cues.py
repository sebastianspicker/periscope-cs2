"""Off-screen edge cues for tracked players (E70).

An outline only helps while the player is on screen. When a track leaves the
frame, this module turns its last bounding box into a clamped edge arrow that
points toward the player. Cues are pure geometry over emitted masks (including
fading ones), capped and sorted by confidence so a crowded frame stays
readable.

Pure and testable; the renderer draws the returned cues with OpenCV.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.domain.predictions import InstanceMask

_MAX_CUES = 8
_MIN_CONFIDENCE = 0.15


@dataclass(frozen=True)
class EdgeCue:
    x: float
    y: float
    angle: float  # radians, direction from the frame centre to the target
    distance: float  # 0..1 of the frame diagonal
    class_name: str
    confidence: float


def _bbox_center(mask: InstanceMask) -> tuple[float, float]:
    xs = [float(point[0]) for point in mask.polygon]
    ys = [float(point[1]) for point in mask.polygon]
    return (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0


def compute_edge_cues(
    masks: Sequence[InstanceMask],
    width: int,
    height: int,
    *,
    margin: float = 28.0,
    max_cues: int = _MAX_CUES,
) -> tuple[EdgeCue, ...]:
    """Return cues for masks whose centre is outside the frame.

    A mask touching the border but still centred inside is not cued: the
    outline is already partly visible.
    """
    if width <= 0 or height <= 0 or max_cues <= 0:
        return ()
    margin = max(1.0, min(margin, min(width, height) / 3.0))
    cx_frame = width / 2.0
    cy_frame = height / 2.0
    diagonal = math.hypot(width, height)

    cues: list[EdgeCue] = []
    for mask in masks:
        if not mask.polygon or mask.confidence < _MIN_CONFIDENCE:
            continue
        cx, cy = _bbox_center(mask)
        if margin <= cx <= width - margin and margin <= cy <= height - margin:
            continue
        dx = cx - cx_frame
        dy = cy - cy_frame
        distance = min(1.0, math.hypot(dx, dy) / diagonal) if diagonal else 0.0
        if distance <= 0.0:
            continue
        x = min(width - margin, max(margin, cx))
        y = min(height - margin, max(margin, cy))
        cues.append(
            EdgeCue(
                x=x,
                y=y,
                angle=math.atan2(dy, dx),
                distance=distance,
                class_name=mask.class_name,
                confidence=mask.confidence,
            )
        )

    cues.sort(key=lambda cue: cue.confidence, reverse=True)
    return tuple(cues[:max_cues])
