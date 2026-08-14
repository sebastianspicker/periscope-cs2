"""Frame evaluation payloads and prediction/dataset loading."""

from __future__ import annotations

import json
import math
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cs2_vision_access.evaluation._io import _require_path_segment
from cs2_vision_access.evaluation.errors import EvaluationError
from cs2_vision_access.evaluation.geometry import (
    SCHEMA_VERSION,
    PolygonInstance,
    _require_positive_raster_area,
    load_yolo_polygons,
)
from cs2_vision_access.predictions import Point


@dataclass(frozen=True)
class FrameEvaluation:
    """Ground truth and predictions for one image."""

    image_id: str
    width: int
    height: int
    ground_truth: tuple[PolygonInstance, ...]
    predictions: tuple[PolygonInstance, ...]

    def __post_init__(self) -> None:
        if not self.image_id.strip():
            raise ValueError("image_id must not be empty")
        _require_path_segment(self.image_id, label="image_id")
        if self.width <= 0 or self.height <= 0:
            raise ValueError("width and height must be positive")
        for index, instance in enumerate(self.ground_truth):
            _require_positive_raster_area(
                instance.polygon,
                height=self.height,
                width=self.width,
                label=f"ground_truth[{index}]",
            )
        for index, instance in enumerate(self.predictions):
            _require_positive_raster_area(
                instance.polygon,
                height=self.height,
                width=self.width,
                label=f"predictions[{index}]",
            )


def load_predictions_json(path: str | Path) -> dict[str, Any]:
    """Load a predictions cache JSON object used by ``eval-masks``."""
    candidate = Path(path)
    if candidate.is_symlink() or not candidate.is_file():
        raise EvaluationError("predictions path must be a regular local file")
    if candidate.suffix.lower() != ".json":
        raise EvaluationError("predictions path must end with .json")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise EvaluationError(f"could not read predictions JSON: {error}") from error
    if not isinstance(payload, dict):
        raise EvaluationError("predictions JSON root must be an object")
    return payload


def frames_from_predictions_and_dataset(
    predictions_payload: Mapping[str, Any],
    dataset_root: str | Path,
    *,
    split: str,
) -> tuple[FrameEvaluation, ...]:
    """Build frame evaluations from a predictions cache and YOLO label split.

    Evaluation universe is the **union** of:

    - stems listed under ``predictions.images``
    - stems of regular ``labels/{split}/*.txt`` files

    Label-only stems (present on disk, absent from the predictions cache) are
    included with empty predictions so their ground-truth instances count as
    false negatives. They require ``default_width`` and ``default_height`` on
    the predictions payload for YOLO denormalization.

    Predictions schema (v1)::

        {
          "schema_version": 1,
          "default_width": 64,
          "default_height": 64,
          "images": {
            "<stem>": {
              "width": 64,
              "height": 64,
              "predictions": [
                {
                  "class_id": 0,
                  "confidence": 0.9,
                  "polygon": [[x, y], ...]
                }
              ]
            }
          }
        }

    Polygons are absolute pixel coordinates. Missing labels for a prediction
    stem yield empty ground truth (all predictions count as false positives).
    """
    version = predictions_payload.get("schema_version")
    if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
        raise EvaluationError(f"predictions schema_version must be {SCHEMA_VERSION}")
    images = predictions_payload.get("images")
    if not isinstance(images, dict):
        raise EvaluationError("predictions.images must be an object")

    root = Path(dataset_root)
    if root.is_symlink() or not root.is_dir():
        raise EvaluationError("dataset root must be a regular directory")
    root_resolved = root.resolve()

    split_name = _require_path_segment(split, label="split")
    labels_split = root / "labels" / split_name
    if labels_split.is_symlink():
        raise EvaluationError(f"labels split must not be a symlink: {labels_split}")
    if not labels_split.is_dir():
        raise EvaluationError(f"labels split is missing: {labels_split}")
    labels_resolved = labels_split.resolve()
    try:
        labels_resolved.relative_to(root_resolved)
    except ValueError as error:
        raise EvaluationError(
            f"labels split resolves outside the dataset root: {labels_split}"
        ) from error

    label_stems = _list_label_stems(labels_split, labels_resolved)
    for image_id in images:
        if not isinstance(image_id, str):
            raise EvaluationError("image ids must be strings")
        _require_path_segment(image_id, label="image_id")

    all_stems = sorted(set(label_stems) | set(images))
    if not all_stems:
        raise EvaluationError(
            "evaluation universe is empty: predictions.images and labels split "
            "both contain no stems"
        )

    default_size = _optional_default_size(predictions_payload)
    frames: list[FrameEvaluation] = []
    for image_id in all_stems:
        entry = images.get(image_id)
        if entry is None:
            if default_size is None:
                raise EvaluationError(
                    f"label stem {image_id!r} has no predictions entry; provide "
                    "default_width and default_height on the predictions payload "
                    "or add an explicit images entry with width/height"
                )
            width, height = default_size
            predictions: tuple[PolygonInstance, ...] = ()
        else:
            if not isinstance(entry, dict):
                raise EvaluationError(f"image entry for {image_id!r} must be an object")
            width, height = _parse_image_size(entry, image_id=image_id)
            raw_predictions = entry.get("predictions", [])
            if not isinstance(raw_predictions, list):
                raise EvaluationError(f"image {image_id!r} predictions must be a list")
            predictions = tuple(
                _parse_prediction_instance(
                    item,
                    image_id=image_id,
                    index=index,
                    width=width,
                    height=height,
                )
                for index, item in enumerate(raw_predictions)
            )

        label_path = _safe_label_path(labels_split, labels_resolved, image_id)
        if label_path.is_symlink():
            raise EvaluationError(f"label path must not be a symlink: {label_path}")
        if label_path.is_file():
            ground_truth = load_yolo_polygons(
                label_path,
                image_width=width,
                image_height=height,
            )
        elif label_path.exists():
            raise EvaluationError(f"label path exists but is not a regular file: {label_path}")
        else:
            ground_truth = ()
        frames.append(
            FrameEvaluation(
                image_id=image_id,
                width=width,
                height=height,
                ground_truth=ground_truth,
                predictions=predictions,
            )
        )
    return tuple(frames)


def _list_label_stems(labels_split: Path, labels_resolved: Path) -> set[str]:
    stems: set[str] = set()
    for path in sorted(labels_split.iterdir()):
        if path.is_symlink():
            raise EvaluationError(f"label path must not be a symlink: {path}")
        if not path.is_file() or path.suffix.lower() != ".txt":
            continue
        stem = path.stem
        _require_path_segment(stem, label="label stem")
        resolved = path.resolve()
        try:
            resolved.relative_to(labels_resolved)
        except ValueError as error:
            raise EvaluationError(
                f"label file resolves outside the labels split: {path}"
            ) from error
        stems.add(stem)
    return stems


def _safe_label_path(labels_split: Path, labels_resolved: Path, image_id: str) -> Path:
    _require_path_segment(image_id, label="image_id")
    label_path = labels_split / f"{image_id}.txt"
    resolved = label_path.resolve()
    try:
        resolved.relative_to(labels_resolved)
    except ValueError as error:
        raise EvaluationError(
            f"label path escapes the labels split directory: {label_path}"
        ) from error
    return label_path


def _optional_default_size(
    payload: Mapping[str, Any],
) -> tuple[int, int] | None:
    if "default_width" not in payload and "default_height" not in payload:
        return None
    width = payload.get("default_width")
    height = payload.get("default_height")
    if (
        isinstance(width, bool)
        or isinstance(height, bool)
        or not isinstance(width, int)
        or not isinstance(height, int)
        or width <= 0
        or height <= 0
    ):
        raise EvaluationError("default_width and default_height must be positive integers")
    return width, height


def _parse_image_size(entry: Mapping[str, Any], *, image_id: str) -> tuple[int, int]:
    width = entry.get("width")
    height = entry.get("height")
    if (
        isinstance(width, bool)
        or isinstance(height, bool)
        or not isinstance(width, int)
        or not isinstance(height, int)
        or width <= 0
        or height <= 0
    ):
        raise EvaluationError(f"image {image_id!r} needs positive integer width/height")
    return width, height


def _parse_prediction_instance(
    raw: object,
    *,
    image_id: str,
    index: int,
    width: int,
    height: int,
) -> PolygonInstance:
    if not isinstance(raw, dict):
        raise EvaluationError(f"{image_id} prediction {index} must be an object")
    class_id = raw.get("class_id")
    if isinstance(class_id, bool) or not isinstance(class_id, int) or class_id < 0:
        raise EvaluationError(f"{image_id} prediction {index} has invalid class_id")
    confidence = raw.get("confidence", 1.0)
    if isinstance(confidence, bool) or not isinstance(confidence, (int, float)):
        raise EvaluationError(f"{image_id} prediction {index} has invalid confidence")
    confidence_value = float(confidence)
    if not math.isfinite(confidence_value) or not 0.0 <= confidence_value <= 1.0:
        raise EvaluationError(f"{image_id} prediction {index} confidence must be in [0, 1]")
    polygon_raw = raw.get("polygon")
    if not isinstance(polygon_raw, list) or len(polygon_raw) < 3:
        raise EvaluationError(f"{image_id} prediction {index} polygon needs at least three points")
    points: list[Point] = []
    for point in polygon_raw:
        if (
            not isinstance(point, (list, tuple))
            or len(point) != 2
            or not all(
                isinstance(value, (int, float)) and not isinstance(value, bool) for value in point
            )
        ):
            raise EvaluationError(
                f"{image_id} prediction {index} polygon points must be [x, y] numbers"
            )
        x_value = float(point[0])
        y_value = float(point[1])
        if not math.isfinite(x_value) or not math.isfinite(y_value):
            raise EvaluationError(
                f"{image_id} prediction {index} polygon coordinates must be finite"
            )
        points.append((x_value, y_value))
    try:
        instance = PolygonInstance(
            class_id=class_id,
            polygon=tuple(points),
            confidence=confidence_value,
        )
        _require_positive_raster_area(
            instance.polygon,
            height=height,
            width=width,
            label="polygon",
        )
    except ValueError as error:
        raise EvaluationError(f"{image_id} prediction {index}: {error}") from error
    return instance
