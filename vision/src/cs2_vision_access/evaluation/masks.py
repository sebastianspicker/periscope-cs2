"""Aggregate mask evaluation and metrics JSON writers."""

from __future__ import annotations

import json
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cs2_vision_access.evaluation.errors import EvaluationError
from cs2_vision_access.evaluation.frames import (
    FrameEvaluation,
    frames_from_predictions_and_dataset,
    load_predictions_json,
)
from cs2_vision_access.evaluation.geometry import (
    DEFAULT_AREA_BINS,
    DEFAULT_BOUNDARY_DILATION_PX,
    DEFAULT_IOU_THRESHOLD,
    SCHEMA_VERSION,
    AreaBinRecall,
    _harmonic_mean,
    _json_float,
    boundary_f1,
    match_instances,
    raster_area_px,
    recall_by_area_bin,
)


@dataclass(frozen=True)
class EvaluationResult:
    """Schema-versioned aggregate metrics for a prediction set."""

    schema_version: int
    image_count: int
    ground_truth_count: int
    prediction_count: int
    true_positives: int
    false_positives: int
    false_negatives: int
    precision: float
    recall: float
    f1: float
    mean_matched_iou: float
    mean_boundary_f1: float
    iou_threshold: float
    boundary_dilation_px: int
    recall_by_area_bin: tuple[AreaBinRecall, ...]

    def as_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "image_count": self.image_count,
            "ground_truth_count": self.ground_truth_count,
            "prediction_count": self.prediction_count,
            "true_positives": self.true_positives,
            "false_positives": self.false_positives,
            "false_negatives": self.false_negatives,
            "precision": _json_float(self.precision),
            "recall": _json_float(self.recall),
            "f1": _json_float(self.f1),
            "mean_matched_iou": _json_float(self.mean_matched_iou),
            "mean_boundary_f1": _json_float(self.mean_boundary_f1),
            "iou_threshold": _json_float(self.iou_threshold),
            "boundary_dilation_px": self.boundary_dilation_px,
            "recall_by_area_bin": [entry.as_dict() for entry in self.recall_by_area_bin],
        }


def evaluate_predictions(
    frames: Sequence[FrameEvaluation],
    *,
    iou_threshold: float = DEFAULT_IOU_THRESHOLD,
    boundary_dilation_px: int = DEFAULT_BOUNDARY_DILATION_PX,
    area_bins: Sequence[tuple[str, float, float]] = DEFAULT_AREA_BINS,
) -> EvaluationResult:
    """Aggregate precision/recall, mean IoU, boundary F1, and area-bin recall."""
    if not 0.0 <= iou_threshold <= 1.0:
        raise EvaluationError("iou_threshold must be in [0, 1]")
    if boundary_dilation_px < 0:
        raise EvaluationError("boundary_dilation_px must be non-negative")

    true_positives = 0
    false_positives = 0
    false_negatives = 0
    ground_truth_count = 0
    prediction_count = 0
    matched_ious: list[float] = []
    matched_boundary_f1: list[float] = []
    all_areas: list[float] = []
    all_matched_offsets: list[int] = []
    gt_offset = 0

    for frame in frames:
        matches = match_instances(
            frame.ground_truth,
            frame.predictions,
            height=frame.height,
            width=frame.width,
            iou_threshold=iou_threshold,
        )
        matched_gt = {gt_index for gt_index, _, _ in matches}
        matched_pred = {pred_index for _, pred_index, _ in matches}
        tp = len(matches)
        fp = len(frame.predictions) - len(matched_pred)
        fn = len(frame.ground_truth) - len(matched_gt)
        true_positives += tp
        false_positives += fp
        false_negatives += fn
        ground_truth_count += len(frame.ground_truth)
        prediction_count += len(frame.predictions)

        for gt_index, pred_index, iou in matches:
            matched_ious.append(iou)
            matched_boundary_f1.append(
                boundary_f1(
                    frame.ground_truth[gt_index].polygon,
                    frame.predictions[pred_index].polygon,
                    height=frame.height,
                    width=frame.width,
                    dilation_px=boundary_dilation_px,
                )
            )
            all_matched_offsets.append(gt_offset + gt_index)

        for instance in frame.ground_truth:
            all_areas.append(
                raster_area_px(
                    instance.polygon,
                    height=frame.height,
                    width=frame.width,
                )
            )
        gt_offset += len(frame.ground_truth)

    precision = (
        float(true_positives) / float(true_positives + false_positives)
        if (true_positives + false_positives)
        else 0.0
    )
    recall = (
        float(true_positives) / float(true_positives + false_negatives)
        if (true_positives + false_negatives)
        else 0.0
    )
    f1 = _harmonic_mean(precision, recall)
    mean_iou = float(sum(matched_ious) / len(matched_ious)) if matched_ious else 0.0
    mean_bf1 = (
        float(sum(matched_boundary_f1) / len(matched_boundary_f1)) if matched_boundary_f1 else 0.0
    )
    area_recall = recall_by_area_bin(
        all_areas,
        all_matched_offsets,
        bins=area_bins,
    )
    return EvaluationResult(
        schema_version=SCHEMA_VERSION,
        image_count=len(frames),
        ground_truth_count=ground_truth_count,
        prediction_count=prediction_count,
        true_positives=true_positives,
        false_positives=false_positives,
        false_negatives=false_negatives,
        precision=precision,
        recall=recall,
        f1=f1,
        mean_matched_iou=mean_iou,
        mean_boundary_f1=mean_bf1,
        iou_threshold=iou_threshold,
        boundary_dilation_px=boundary_dilation_px,
        recall_by_area_bin=area_recall,
    )


def evaluate_masks_from_files(
    *,
    predictions_path: str | Path,
    dataset_root: str | Path,
    split: str,
    iou_threshold: float = DEFAULT_IOU_THRESHOLD,
    boundary_dilation_px: int = DEFAULT_BOUNDARY_DILATION_PX,
) -> EvaluationResult:
    """Load predictions JSON + dataset labels and compute aggregate metrics."""
    payload = load_predictions_json(predictions_path)
    frames = frames_from_predictions_and_dataset(
        payload,
        dataset_root,
        split=split,
    )
    return evaluate_predictions(
        frames,
        iou_threshold=iou_threshold,
        boundary_dilation_px=boundary_dilation_px,
    )


def write_metrics_json(payload: Mapping[str, Any], path: str | Path) -> Path:
    """Write a schema-versioned metrics object as sorted-key JSON."""
    if not isinstance(payload, Mapping):
        raise EvaluationError("metrics payload must be a mapping")
    output = Path(path)
    if output.is_symlink():
        raise EvaluationError("output path must not be a symlink")
    if output.suffix.lower() != ".json":
        raise EvaluationError("output path must end with .json")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.parent.is_symlink():
        raise EvaluationError("output parent must not be a symlink")
    text = json.dumps(dict(payload), indent=2, sort_keys=True) + "\n"
    output.write_text(text, encoding="utf-8")
    return output


def write_evaluation_json(result: EvaluationResult, path: str | Path) -> Path:
    """Write an evaluation result as sorted-key JSON."""
    return write_metrics_json(result.as_dict(), path)
