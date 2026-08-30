"""Temporal contour instability metrics for offline sequences."""

from __future__ import annotations

import json
import math
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.domain.predictions import Point
from cs2_vision_access.workflows.evaluation.errors import EvaluationError
from cs2_vision_access.workflows.evaluation.frames import _parse_prediction_instance
from cs2_vision_access.workflows.evaluation.geometry import (
    DEFAULT_IOU_THRESHOLD,
    SCHEMA_VERSION,
    PolygonInstance,
    _json_float,
    match_instances,
)


@dataclass(frozen=True)
class TemporalFrame:
    """One frame of offline predicted instances for temporal metrics."""

    frame_index: int
    width: int
    height: int
    predictions: tuple[PolygonInstance, ...]

    def __post_init__(self) -> None:
        if isinstance(self.frame_index, bool) or not isinstance(self.frame_index, int):
            raise ValueError("frame_index must be an integer")
        if self.frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if self.width <= 0 or self.height <= 0:
            raise ValueError("width and height must be positive")


@dataclass(frozen=True)
class TemporalEvaluationResult:
    """Frame-to-frame contour instability proxies."""

    schema_version: int
    frame_count: int
    consecutive_pair_count: int
    matched_pair_count: int
    mean_centroid_displacement_px: float
    mean_iou_drop: float
    presence_flicker_rate: float
    iou_threshold: float

    def as_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "frame_count": self.frame_count,
            "consecutive_pair_count": self.consecutive_pair_count,
            "matched_pair_count": self.matched_pair_count,
            "mean_centroid_displacement_px": _json_float(self.mean_centroid_displacement_px),
            "mean_iou_drop": _json_float(self.mean_iou_drop),
            "presence_flicker_rate": _json_float(self.presence_flicker_rate),
            "iou_threshold": _json_float(self.iou_threshold),
        }


def polygon_centroid(polygon: Sequence[Point]) -> tuple[float, float]:
    """Area-weighted polygon centroid (shoelace); falls back to vertex mean."""
    points = list(polygon)
    if not points:
        raise EvaluationError("polygon must not be empty")
    if len(points) == 1:
        return float(points[0][0]), float(points[0][1])
    if len(points) == 2:
        return (
            (float(points[0][0]) + float(points[1][0])) / 2.0,
            (float(points[0][1]) + float(points[1][1])) / 2.0,
        )
    area_sum = 0.0
    cx_sum = 0.0
    cy_sum = 0.0
    for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1], strict=False):
        cross = x1 * y2 - x2 * y1
        area_sum += cross
        cx_sum += (x1 + x2) * cross
        cy_sum += (y1 + y2) * cross
    area = area_sum / 2.0
    if abs(area) < 1e-12:
        xs = [float(x) for x, _ in points]
        ys = [float(y) for _, y in points]
        return sum(xs) / len(xs), sum(ys) / len(ys)
    cx = cx_sum / (6.0 * area)
    cy = cy_sum / (6.0 * area)
    return float(cx), float(cy)


def evaluate_temporal(
    frames: Sequence[TemporalFrame],
    *,
    iou_threshold: float = DEFAULT_IOU_THRESHOLD,
) -> TemporalEvaluationResult:
    """Match consecutive-frame instances by IoU; report instability proxies.

    Metrics (no acceptance thresholds):

    - ``mean_centroid_displacement_px``: mean L2 centroid shift for matched pairs
    - ``mean_iou_drop``: mean of ``1 - IoU`` for matched pairs
    - ``presence_flicker_rate``: unmatched instance endpoints over consecutive
      pairs, divided by total instance endpoints (0 when no instances)
    """
    if not 0.0 <= iou_threshold <= 1.0:
        raise EvaluationError("iou_threshold must be in [0, 1]")

    ordered = sorted(frames, key=lambda frame: frame.frame_index)
    if len(ordered) >= 2:
        indices = [frame.frame_index for frame in ordered]
        if len(indices) != len(set(indices)):
            raise EvaluationError("frame_index values must be unique within a sequence")

    displacements: list[float] = []
    iou_drops: list[float] = []
    matched_pair_count = 0
    flicker_events = 0
    presence_slots = 0
    consecutive_pair_count = max(0, len(ordered) - 1)

    for left, right in zip(ordered, ordered[1:], strict=False):
        # Match across frames using the same greedy same-class IoU matcher.
        # Raster size: use the max dimensions so both polygons fit.
        height = max(left.height, right.height)
        width = max(left.width, right.width)
        matches = match_instances(
            left.predictions,
            right.predictions,
            height=height,
            width=width,
            iou_threshold=iou_threshold,
        )
        matched_left = {gt_index for gt_index, _, _ in matches}
        matched_right = {pred_index for _, pred_index, _ in matches}
        matched_pair_count += len(matches)
        for left_index, right_index, iou in matches:
            c0 = polygon_centroid(left.predictions[left_index].polygon)
            c1 = polygon_centroid(right.predictions[right_index].polygon)
            dx = c1[0] - c0[0]
            dy = c1[1] - c0[1]
            displacements.append(math.hypot(dx, dy))
            iou_drops.append(1.0 - float(iou))

        unmatched_left = len(left.predictions) - len(matched_left)
        unmatched_right = len(right.predictions) - len(matched_right)
        flicker_events += unmatched_left + unmatched_right
        presence_slots += len(left.predictions) + len(right.predictions)

    mean_disp = float(sum(displacements) / len(displacements)) if displacements else 0.0
    mean_drop = float(sum(iou_drops) / len(iou_drops)) if iou_drops else 0.0
    flicker = float(flicker_events) / float(presence_slots) if presence_slots else 0.0

    return TemporalEvaluationResult(
        schema_version=SCHEMA_VERSION,
        frame_count=len(ordered),
        consecutive_pair_count=consecutive_pair_count,
        matched_pair_count=matched_pair_count,
        mean_centroid_displacement_px=mean_disp,
        mean_iou_drop=mean_drop,
        presence_flicker_rate=flicker,
        iou_threshold=float(iou_threshold),
    )


def load_temporal_sequence_json(path: str | Path) -> tuple[TemporalFrame, ...]:
    """Load a temporal sequence JSON (schema v1 object or bare frame list)."""
    candidate = Path(path)
    if candidate.is_symlink() or not candidate.is_file():
        raise EvaluationError("sequence path must be a regular local file")
    if candidate.suffix.lower() != ".json":
        raise EvaluationError("sequence path must end with .json")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise EvaluationError(f"could not read sequence JSON: {error}") from error
    return temporal_frames_from_payload(payload)


def temporal_frames_from_payload(payload: object) -> tuple[TemporalFrame, ...]:
    """Parse temporal frames from a schema v1 object or list of frame objects."""
    raw_frames: list[object]
    if isinstance(payload, list):
        raw_frames = payload
    elif isinstance(payload, dict):
        version = payload.get("schema_version", SCHEMA_VERSION)
        if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
            raise EvaluationError(f"sequence schema_version must be {SCHEMA_VERSION}")
        frames_value = payload.get("frames")
        if not isinstance(frames_value, list):
            raise EvaluationError("sequence.frames must be a list")
        raw_frames = frames_value
    else:
        raise EvaluationError("sequence JSON root must be an object or list")

    frames: list[TemporalFrame] = []
    for index, raw in enumerate(raw_frames):
        if not isinstance(raw, dict):
            raise EvaluationError(f"frames[{index}] must be an object")
        frame_index = raw.get("frame_index", index)
        if isinstance(frame_index, bool) or not isinstance(frame_index, int) or frame_index < 0:
            raise EvaluationError(f"frames[{index}].frame_index must be a non-negative integer")
        width = raw.get("width")
        height = raw.get("height")
        if (
            isinstance(width, bool)
            or isinstance(height, bool)
            or not isinstance(width, int)
            or not isinstance(height, int)
            or width <= 0
            or height <= 0
        ):
            raise EvaluationError(f"frames[{index}] needs positive integer width/height")
        raw_predictions = raw.get("predictions", [])
        if not isinstance(raw_predictions, list):
            raise EvaluationError(f"frames[{index}].predictions must be a list")
        predictions = tuple(
            _parse_prediction_instance(
                item,
                image_id=f"frame_{frame_index}",
                index=pred_index,
                width=width,
                height=height,
            )
            for pred_index, item in enumerate(raw_predictions)
        )
        try:
            frames.append(
                TemporalFrame(
                    frame_index=frame_index,
                    width=width,
                    height=height,
                    predictions=predictions,
                )
            )
        except ValueError as error:
            raise EvaluationError(f"frames[{index}]: {error}") from error
    return tuple(frames)


def evaluate_temporal_from_files(
    *,
    sequence_path: str | Path,
    iou_threshold: float = DEFAULT_IOU_THRESHOLD,
) -> TemporalEvaluationResult:
    """Load a sequence JSON and compute temporal instability metrics."""
    frames = load_temporal_sequence_json(sequence_path)
    return evaluate_temporal(frames, iou_threshold=iou_threshold)
