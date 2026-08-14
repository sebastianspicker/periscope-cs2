"""Mask and accessibility evaluation metrics for offline CS2 research.

Public API for the ``cs2_vision_access.evaluation`` package (mask, temporal,
negative, and comfort metrics).
"""

from __future__ import annotations

from cs2_vision_access.evaluation.comfort import (
    DEFAULT_BACKGROUND_RADIUS_PX,
    DEFAULT_CONTRAST_SAMPLE_SPACING_PX,
    DEFAULT_OUTWARD_OFFSET_PX,
    DEFAULT_STROKE_CONTRAST_THRESHOLD,
    ComfortEvaluationResult,
    clutter_fraction,
    evaluate_comfort,
    evaluate_comfort_from_files,
    load_comfort_predictions_json,
    local_stroke_contrast_samples,
)
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
    PolygonInstance,
    boundary_f1,
    load_yolo_polygons,
    mask_iou,
    match_instances,
    polygon_area_px,
    raster_area_px,
    rasterize_polygon,
    recall_by_area_bin,
)
from cs2_vision_access.evaluation.masks import (
    EvaluationResult,
    evaluate_masks_from_files,
    evaluate_predictions,
    write_evaluation_json,
    write_metrics_json,
)
from cs2_vision_access.evaluation.negatives import (
    NegativesEvaluationResult,
    evaluate_negatives,
    evaluate_negatives_from_files,
    negatives_counts_from_predictions,
)
from cs2_vision_access.evaluation.temporal import (
    TemporalEvaluationResult,
    TemporalFrame,
    evaluate_temporal,
    evaluate_temporal_from_files,
    load_temporal_sequence_json,
    polygon_centroid,
    temporal_frames_from_payload,
)

__all__ = [
    "SCHEMA_VERSION",
    "DEFAULT_IOU_THRESHOLD",
    "DEFAULT_BOUNDARY_DILATION_PX",
    "DEFAULT_AREA_BINS",
    "DEFAULT_STROKE_CONTRAST_THRESHOLD",
    "DEFAULT_CONTRAST_SAMPLE_SPACING_PX",
    "DEFAULT_OUTWARD_OFFSET_PX",
    "DEFAULT_BACKGROUND_RADIUS_PX",
    "EvaluationError",
    "PolygonInstance",
    "FrameEvaluation",
    "AreaBinRecall",
    "EvaluationResult",
    "NegativesEvaluationResult",
    "TemporalFrame",
    "TemporalEvaluationResult",
    "ComfortEvaluationResult",
    "load_yolo_polygons",
    "rasterize_polygon",
    "mask_iou",
    "match_instances",
    "boundary_f1",
    "polygon_area_px",
    "raster_area_px",
    "recall_by_area_bin",
    "evaluate_predictions",
    "load_predictions_json",
    "frames_from_predictions_and_dataset",
    "evaluate_masks_from_files",
    "write_metrics_json",
    "write_evaluation_json",
    "evaluate_negatives",
    "negatives_counts_from_predictions",
    "evaluate_negatives_from_files",
    "polygon_centroid",
    "evaluate_temporal",
    "load_temporal_sequence_json",
    "temporal_frames_from_payload",
    "evaluate_temporal_from_files",
    "clutter_fraction",
    "local_stroke_contrast_samples",
    "evaluate_comfort",
    "load_comfort_predictions_json",
    "evaluate_comfort_from_files",
]
