"""False-positive metrics on expected-empty (negative) frames."""

from __future__ import annotations

import math
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.evaluation._io import _require_path_segment
from cs2_vision_access.workflows.evaluation.errors import EvaluationError
from cs2_vision_access.workflows.evaluation.frames import (
    _parse_prediction_instance,
    frames_from_predictions_and_dataset,
    load_predictions_json,
)
from cs2_vision_access.workflows.evaluation.geometry import SCHEMA_VERSION, _json_float


@dataclass(frozen=True)
class NegativesEvaluationResult:
    """False-positive rate on frames expected to contain no instances."""

    schema_version: int
    frame_count: int
    false_positive_count: int
    fps: float
    duration_seconds: float
    false_positives_per_minute: float
    selection: str

    def as_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "frame_count": self.frame_count,
            "false_positive_count": self.false_positive_count,
            "fps": _json_float(self.fps),
            "duration_seconds": _json_float(self.duration_seconds),
            "false_positives_per_minute": _json_float(self.false_positives_per_minute),
            "selection": self.selection,
        }


def evaluate_negatives(
    false_positive_counts: Sequence[int],
    *,
    fps: float,
    selection: str = "expected_empty",
) -> NegativesEvaluationResult:
    """Aggregate FP counts on expected-empty frames into FP-per-minute.

    ``false_positives_per_minute = fp_count / (frames / fps) * 60``.
    Empty input yields finite zero rates.
    """
    if isinstance(fps, bool) or not isinstance(fps, (int, float)):
        raise EvaluationError("fps must be a positive finite number")
    fps_value = float(fps)
    if not math.isfinite(fps_value) or fps_value <= 0.0:
        raise EvaluationError("fps must be a positive finite number")
    if not selection or not isinstance(selection, str):
        raise EvaluationError("selection must be a non-empty string")

    counts = list(false_positive_counts)
    for index, count in enumerate(counts):
        if isinstance(count, bool) or not isinstance(count, int) or count < 0:
            raise EvaluationError(f"false_positive_counts[{index}] must be a non-negative integer")

    frame_count = len(counts)
    fp_count = int(sum(counts))
    if frame_count == 0:
        duration_seconds = 0.0
        fp_per_minute = 0.0
    else:
        duration_seconds = float(frame_count) / fps_value
        fp_per_minute = float(fp_count) / duration_seconds * 60.0

    return NegativesEvaluationResult(
        schema_version=SCHEMA_VERSION,
        frame_count=frame_count,
        false_positive_count=fp_count,
        fps=fps_value,
        duration_seconds=duration_seconds,
        false_positives_per_minute=fp_per_minute,
        selection=selection,
    )


def negatives_counts_from_predictions(
    predictions_payload: Mapping[str, Any],
    dataset_root: str | Path | None = None,
    *,
    split: str = "test",
) -> tuple[tuple[int, ...], str]:
    """Collect per-frame FP counts on expected-empty frames.

    A frame is expected empty when:

    - its predictions entry has ``expected_zero: true``, and/or
    - a YOLO label file exists under ``labels/{split}/{stem}.txt`` and parses
      to zero instances (empty file is valid).

    When ``dataset_root`` is omitted, only ``expected_zero`` entries are used.
    When only a dataset is used for selection, predictions still supply the
    predicted instance lists (missing stems → zero predictions).
    """
    version = predictions_payload.get("schema_version")
    if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
        raise EvaluationError(f"predictions schema_version must be {SCHEMA_VERSION}")
    images = predictions_payload.get("images")
    if not isinstance(images, dict):
        raise EvaluationError("predictions.images must be an object")

    expected_zero_stems: set[str] = set()
    for image_id, entry in images.items():
        if not isinstance(image_id, str):
            raise EvaluationError("image ids must be strings")
        _require_path_segment(image_id, label="image_id")
        if not isinstance(entry, dict):
            raise EvaluationError(f"image entry for {image_id!r} must be an object")
        flag = entry.get("expected_zero", False)
        if isinstance(flag, bool) and flag:
            expected_zero_stems.add(image_id)
        elif flag not in (False, None):
            raise EvaluationError(
                f"image {image_id!r} expected_zero must be a boolean when present"
            )

    empty_label_stems: set[str] = set()
    if dataset_root is not None:
        empty_label_stems = _empty_label_stems(dataset_root, split=split)

    if expected_zero_stems and empty_label_stems:
        selected = sorted(expected_zero_stems | empty_label_stems)
        selection = "expected_zero_and_empty_labels"
    elif expected_zero_stems:
        selected = sorted(expected_zero_stems)
        selection = "expected_zero"
    elif empty_label_stems:
        selected = sorted(empty_label_stems)
        selection = "empty_labels"
    else:
        raise EvaluationError(
            "no expected-empty frames: mark predictions.images[*].expected_zero "
            "or provide --dataset-root with empty YOLO label files"
        )

    # Reuse frame builder when a dataset is available so label/prediction
    # path safety and size defaults stay consistent with eval-masks.
    if dataset_root is not None:
        frames = frames_from_predictions_and_dataset(
            predictions_payload,
            dataset_root,
            split=split,
        )
        by_id = {frame.image_id: frame for frame in frames}
        counts: list[int] = []
        for stem in selected:
            frame = by_id.get(stem)
            if frame is None:
                counts.append(0)
            else:
                counts.append(len(frame.predictions))
        return tuple(counts), selection

    counts = []
    for stem in selected:
        entry = images[stem]
        raw_predictions = entry.get("predictions", [])
        if not isinstance(raw_predictions, list):
            raise EvaluationError(f"image {stem!r} predictions must be a list")
        # Validate geometry without requiring width/height for count-only path
        # when the list is empty; non-empty needs size for raster checks.
        if not raw_predictions:
            counts.append(0)
            continue
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
            raise EvaluationError(f"image {stem!r} needs positive integer width/height")
        parsed = tuple(
            _parse_prediction_instance(
                item,
                image_id=stem,
                index=index,
                width=width,
                height=height,
            )
            for index, item in enumerate(raw_predictions)
        )
        counts.append(len(parsed))
    return tuple(counts), selection


def evaluate_negatives_from_files(
    *,
    predictions_path: str | Path,
    fps: float,
    dataset_root: str | Path | None = None,
    split: str = "test",
) -> NegativesEvaluationResult:
    """Load predictions (+ optional empty labels) and compute FP-per-minute."""
    payload = load_predictions_json(predictions_path)
    counts, selection = negatives_counts_from_predictions(
        payload,
        dataset_root,
        split=split,
    )
    return evaluate_negatives(counts, fps=fps, selection=selection)


def _empty_label_stems(dataset_root: str | Path, *, split: str) -> set[str]:
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

    empty: set[str] = set()
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
        # Empty labels: no non-whitespace tokens. Avoid geometry parse at a
        # dummy size (non-empty labels may fail sub-pixel raster checks).
        try:
            contents = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            raise EvaluationError(f"could not read label file: {error}") from error
        if not any(line.split() for line in contents.splitlines()):
            empty.add(stem)
    return empty
