"""Mask geometry metrics for offline CS2 instance-segmentation research.

Pure geometry functions compare ground-truth YOLO polygons to predicted
polygons. No model loading or GPU is required. Metrics map to the RESEARCH
evaluation contract (mask IoU matching, boundary F1, area-binned recall)
without inventing acceptance thresholds.

Rasterization lives in :mod:`cs2_vision_access.evaluation.raster` and is
re-exported here so existing metric call sites keep a single import path.
"""

from __future__ import annotations

import math
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from cs2_vision_access.evaluation.errors import EvaluationError
from cs2_vision_access.evaluation.raster import (
    dilate,
    mask_boundary,
    rasterize_polygon,
)
from cs2_vision_access.predictions import Point

SCHEMA_VERSION = 1
DEFAULT_IOU_THRESHOLD = 0.5
DEFAULT_BOUNDARY_DILATION_PX = 2

# Visible-area bins by raster foreground pixel count (inclusive min, exclusive max).
DEFAULT_AREA_BINS: tuple[tuple[str, float, float], ...] = (
    ("tiny", 0.0, 1_024.0),
    ("small", 1_024.0, 4_096.0),
    ("medium", 4_096.0, 16_384.0),
    ("large", 16_384.0, math.inf),
)


@dataclass(frozen=True)
class PolygonInstance:
    """One instance polygon in absolute pixel coordinates."""

    class_id: int
    polygon: tuple[Point, ...]
    confidence: float = 1.0

    def __post_init__(self) -> None:
        if self.class_id < 0:
            raise ValueError("class_id must be non-negative")
        if len(self.polygon) < 3:
            raise ValueError("polygon must contain at least three points")
        if not 0.0 <= self.confidence <= 1.0:
            raise ValueError("confidence must be in [0, 1]")
        for x, y in self.polygon:
            if not math.isfinite(x) or not math.isfinite(y):
                raise ValueError("polygon coordinates must be finite")
        if polygon_area_px(self.polygon) <= 0.0:
            raise ValueError("polygon area must be positive")


@dataclass(frozen=True)
class AreaBinRecall:
    name: str
    min_area: float
    max_area: float
    ground_truth_count: int
    true_positives: int
    recall: float

    def as_dict(self) -> dict[str, object]:
        return {
            "name": self.name,
            "min_area": _json_float(self.min_area),
            "max_area": _json_float(self.max_area),
            "ground_truth_count": self.ground_truth_count,
            "true_positives": self.true_positives,
            "recall": _json_float(self.recall),
        }


def load_yolo_polygons(
    label_path: str | Path,
    *,
    image_width: int,
    image_height: int,
) -> tuple[PolygonInstance, ...]:
    """Load YOLO segmentation labels and convert to pixel-space polygons.

    Format per non-empty line: ``class_id x1 y1 x2 y2 ...`` with coordinates
    normalized to ``[0, 1]``. Empty files are valid (no instances).
    """
    if image_width <= 0 or image_height <= 0:
        raise EvaluationError("image_width and image_height must be positive")
    path = Path(label_path)
    if path.is_symlink() or not path.is_file():
        raise EvaluationError(f"label path must be a regular file: {path}")
    try:
        contents = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise EvaluationError(f"could not read label file: {error}") from error

    instances: list[PolygonInstance] = []
    for line_number, row in enumerate(contents.splitlines(), start=1):
        tokens = row.split()
        if not tokens:
            continue
        if len(tokens) < 7 or (len(tokens) - 1) % 2:
            raise EvaluationError(
                f"{path}:{line_number}: row needs a class id and at least three coordinate pairs"
            )
        class_token = tokens[0]
        if not class_token.isascii() or not class_token.isdecimal():
            raise EvaluationError(f"{path}:{line_number}: class id must be a non-negative integer")
        class_id = int(class_token)
        try:
            coordinates = [float(token) for token in tokens[1:]]
        except ValueError as error:
            raise EvaluationError(
                f"{path}:{line_number}: coordinates must be finite decimal numbers"
            ) from error
        if any(not math.isfinite(value) or not 0.0 <= value <= 1.0 for value in coordinates):
            raise EvaluationError(
                f"{path}:{line_number}: coordinates must be finite values in [0, 1]"
            )
        points = tuple(
            (coordinates[index] * image_width, coordinates[index + 1] * image_height)
            for index in range(0, len(coordinates), 2)
        )
        try:
            instance = PolygonInstance(class_id=class_id, polygon=points)
            _require_positive_raster_area(
                instance.polygon,
                height=image_height,
                width=image_width,
                label="polygon",
            )
        except ValueError as error:
            raise EvaluationError(f"{path}:{line_number}: {error}") from error
        instances.append(instance)
    return tuple(instances)


def mask_iou(
    first: Sequence[Point],
    second: Sequence[Point],
    *,
    height: int,
    width: int,
) -> float:
    """Intersection-over-union of two polygons after rasterization.

    Empty-empty (union zero) returns ``0.0`` so degenerate geometry never
    scores as a perfect match.
    """
    first_mask = rasterize_polygon(first, height=height, width=width)
    second_mask = rasterize_polygon(second, height=height, width=width)
    intersection = int(np.logical_and(first_mask, second_mask).sum())
    union = int(np.logical_or(first_mask, second_mask).sum())
    if union == 0:
        return 0.0
    return float(intersection) / float(union)


def match_instances(
    ground_truth: Sequence[PolygonInstance],
    predictions: Sequence[PolygonInstance],
    *,
    height: int,
    width: int,
    iou_threshold: float = DEFAULT_IOU_THRESHOLD,
) -> tuple[tuple[int, int, float], ...]:
    """Greedy one-to-one matching by descending IoU (same class only).

    Returns ``(gt_index, pred_index, iou)`` triples for pairs whose IoU is at
    least ``iou_threshold`` and strictly greater than zero (empty-empty and
    fully disjoint pairs never match, even when ``iou_threshold`` is 0).
    """
    if not 0.0 <= iou_threshold <= 1.0:
        raise EvaluationError("iou_threshold must be in [0, 1]")
    candidates: list[tuple[float, int, int]] = []
    for gt_index, gt in enumerate(ground_truth):
        for pred_index, pred in enumerate(predictions):
            if gt.class_id != pred.class_id:
                continue
            iou = mask_iou(
                gt.polygon,
                pred.polygon,
                height=height,
                width=width,
            )
            if iou > 0.0 and iou >= iou_threshold:
                candidates.append((iou, gt_index, pred_index))
    candidates.sort(key=lambda item: (-item[0], item[1], item[2]))
    used_gt: set[int] = set()
    used_pred: set[int] = set()
    matches: list[tuple[int, int, float]] = []
    for iou, gt_index, pred_index in candidates:
        if gt_index in used_gt or pred_index in used_pred:
            continue
        used_gt.add(gt_index)
        used_pred.add(pred_index)
        matches.append((gt_index, pred_index, iou))
    matches.sort(key=lambda item: (item[0], item[1]))
    return tuple(matches)


def boundary_f1(
    first: Sequence[Point],
    second: Sequence[Point],
    *,
    height: int,
    width: int,
    dilation_px: int = DEFAULT_BOUNDARY_DILATION_PX,
) -> float:
    """Boundary F1 between two polygons with symmetric boundary dilation.

    Both-empty boundaries return ``0.0`` (aligned with empty-empty mask IoU).
    """
    if dilation_px < 0:
        raise EvaluationError("dilation_px must be non-negative")
    first_mask = rasterize_polygon(first, height=height, width=width)
    second_mask = rasterize_polygon(second, height=height, width=width)
    first_boundary = mask_boundary(first_mask)
    second_boundary = mask_boundary(second_mask)
    if not first_boundary.any() and not second_boundary.any():
        return 0.0
    if not first_boundary.any() or not second_boundary.any():
        return 0.0

    first_dilated = dilate(first_boundary, dilation_px)
    second_dilated = dilate(second_boundary, dilation_px)
    precision_hits = int(np.logical_and(second_boundary, first_dilated).sum())
    recall_hits = int(np.logical_and(first_boundary, second_dilated).sum())
    precision_denom = int(second_boundary.sum())
    recall_denom = int(first_boundary.sum())
    precision = float(precision_hits) / float(precision_denom) if precision_denom else 0.0
    recall = float(recall_hits) / float(recall_denom) if recall_denom else 0.0
    return _harmonic_mean(precision, recall)


def polygon_area_px(polygon: Sequence[Point]) -> float:
    """Shoelace area of a polygon in pixel units (absolute value)."""
    points = list(polygon)
    if len(points) < 3:
        return 0.0
    total = 0.0
    for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1], strict=False):
        total += x1 * y2 - x2 * y1
    return abs(total) / 2.0


def raster_area_px(
    polygon: Sequence[Point],
    *,
    height: int,
    width: int,
) -> float:
    """Foreground pixel count after rasterization (visible area for binning)."""
    mask = rasterize_polygon(polygon, height=height, width=width)
    return float(mask.sum())


def recall_by_area_bin(
    areas: Sequence[float],
    matched_gt_indices: Iterable[int],
    *,
    bins: Sequence[tuple[str, float, float]] = DEFAULT_AREA_BINS,
) -> tuple[AreaBinRecall, ...]:
    """Compute recall stratified by per-instance visible (raster) area."""
    matched = set(matched_gt_indices)
    results: list[AreaBinRecall] = []
    for name, min_area, max_area in bins:
        if min_area < 0 or max_area <= min_area:
            raise EvaluationError(f"invalid area bin bounds for {name!r}")
        gt_indices = [index for index, area in enumerate(areas) if min_area <= area < max_area]
        true_positives = sum(1 for index in gt_indices if index in matched)
        count = len(gt_indices)
        recall = float(true_positives) / float(count) if count else 0.0
        results.append(
            AreaBinRecall(
                name=name,
                min_area=min_area,
                max_area=max_area,
                ground_truth_count=count,
                true_positives=true_positives,
                recall=recall,
            )
        )
    return tuple(results)


def _require_positive_raster_area(
    polygon: Sequence[Point],
    *,
    height: int,
    width: int,
    label: str,
) -> None:
    """Reject sub-pixel / empty-raster polygons when frame size is known."""
    if raster_area_px(polygon, height=height, width=width) <= 0.0:
        raise ValueError(
            f"{label} raster area must be positive at {width}x{height} "
            "(sub-pixel or empty after rasterization)"
        )


def _harmonic_mean(precision: float, recall: float) -> float:
    if precision <= 0.0 or recall <= 0.0:
        return 0.0
    return 2.0 * precision * recall / (precision + recall)


def _json_float(value: float) -> float | None:
    """Serialize floats with inf/nan mapped to null for JSON finiteness."""
    if not math.isfinite(value):
        return None
    return float(value)


# Back-compat aliases for pre-split private names imported by sibling modules.
_mask_boundary = mask_boundary
_dilate_mask = dilate
