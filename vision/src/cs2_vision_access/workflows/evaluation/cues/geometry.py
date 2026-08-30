"""Geometry helpers for cue tracking (centroids and observation extraction)."""

from __future__ import annotations

import math
from collections.abc import Sequence

from cs2_vision_access.domain.predictions import InstanceMask, Point
from cs2_vision_access.workflows.evaluation.cues.errors import CueLogError


def mask_centroid(polygon: Sequence[Point]) -> tuple[float, float]:
    """Return the arithmetic mean of polygon vertices (pixel space)."""
    if len(polygon) < 1:
        raise CueLogError("polygon must contain at least one point")
    total_x = 0.0
    total_y = 0.0
    for point in polygon:
        if (
            not isinstance(point, tuple)
            or len(point) != 2
            or not all(isinstance(value, (int, float)) for value in point)
        ):
            raise CueLogError("polygon points must be (x, y) number pairs")
        x_value = float(point[0])
        y_value = float(point[1])
        if not math.isfinite(x_value) or not math.isfinite(y_value):
            raise CueLogError("polygon coordinates must be finite")
        total_x += x_value
        total_y += y_value
    count = float(len(polygon))
    return total_x / count, total_y / count


def _observation_from_mask(mask: InstanceMask) -> dict[str, object]:
    centroid_x, centroid_y = mask_centroid(mask.polygon)
    return {
        "class_id": mask.class_id,
        "class_name": mask.class_name,
        "confidence": float(mask.confidence),
        "centroid_x": centroid_x,
        "centroid_y": centroid_y,
    }


def _finite_float(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise CueLogError(f"{field} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise CueLogError(f"{field} must be finite")
    return number
