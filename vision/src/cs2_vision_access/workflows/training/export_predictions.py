"""Export ONNX segmenter predictions to eval-masks JSON cache (schema v1)."""

from __future__ import annotations

import json
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from cs2_vision_access.application.ports.segmentation import Segmenter, create_segmenter
from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.workflows.evaluation.geometry import SCHEMA_VERSION
from cs2_vision_access.workflows.evaluation.image_io import _load_frame_bgr


class ExportPredictionsError(ValueError):
    """Export inputs failed a deterministic check."""


_FLAT_SPLITS = frozenset({"", "all", "flat"})


def resolve_images_dir(dataset_root: str | Path, split: str = "val") -> Path:
    """Resolve the images directory for a dataset split.

    Prefers ``dataset_root/images/{split}/`` when that directory exists.
    Falls back to ``dataset_root/images/`` when ``split`` is empty, ``all``,
    or ``flat`` (flat YOLO layouts without split subfolders).
    """
    root = Path(dataset_root)
    if root.is_symlink() or not root.is_dir():
        raise ExportPredictionsError("dataset root must be a regular directory")

    split_name = (split or "").strip()
    images_root = root / "images"
    if images_root.is_symlink():
        raise ExportPredictionsError(f"images directory must not be a symlink: {images_root}")

    if split_name and split_name.casefold() not in _FLAT_SPLITS:
        split_dir = images_root / split_name
        if split_dir.is_symlink():
            raise ExportPredictionsError(f"images split must not be a symlink: {split_dir}")
        if split_dir.is_dir():
            return split_dir
        raise ExportPredictionsError(
            f"images split is missing: {split_dir} (use --split all/flat for flat images/ layouts)"
        )

    if not images_root.is_dir():
        raise ExportPredictionsError(f"images directory is missing: {images_root}")
    return images_root


def collect_image_paths(images_dir: str | Path, *, max_images: int = 0) -> list[Path]:
    """List image files under ``images_dir`` (non-recursive, stable order).

    ``max_images`` <= 0 means unlimited.
    """
    if isinstance(max_images, bool) or not isinstance(max_images, int) or max_images < 0:
        raise ExportPredictionsError("max_images must be an integer >= 0")

    directory = Path(images_dir)
    if directory.is_symlink() or not directory.is_dir():
        raise ExportPredictionsError(f"images_dir must be a real directory: {directory}")

    files: list[Path] = []
    for path in sorted(directory.iterdir()):
        if path.is_symlink():
            continue
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            files.append(path)

    if max_images > 0:
        files = files[:max_images]
    return files


def instance_to_prediction_dict(instance: InstanceMask) -> dict[str, Any]:
    """Convert an :class:`InstanceMask` to an eval-masks prediction object."""
    return {
        "class_id": int(instance.class_id),
        "confidence": float(instance.confidence),
        "polygon": [[float(x), float(y)] for x, y in instance.polygon],
    }


def predictions_payload_from_instances(
    image_entries: Sequence[tuple[str, int, int, Sequence[InstanceMask]]],
) -> dict[str, Any]:
    """Build a schema v1 predictions cache object.

    Each entry is ``(stem, width, height, instances)``.
    """
    images: dict[str, Any] = {}
    default_width: int | None = None
    default_height: int | None = None

    for stem, width, height, instances in image_entries:
        if not isinstance(stem, str) or not stem or Path(stem).name != stem:
            raise ExportPredictionsError(f"invalid image stem: {stem!r}")
        if "/" in stem or "\\" in stem or stem in {".", ".."}:
            raise ExportPredictionsError(f"invalid image stem: {stem!r}")
        if width <= 0 or height <= 0:
            raise ExportPredictionsError(f"image {stem!r} needs positive width/height")
        if default_width is None:
            default_width = int(width)
            default_height = int(height)
        images[stem] = {
            "width": int(width),
            "height": int(height),
            "predictions": [instance_to_prediction_dict(inst) for inst in instances],
        }

    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "images": images,
    }
    if default_width is not None and default_height is not None:
        payload["default_width"] = default_width
        payload["default_height"] = default_height
    return payload


def export_predictions(
    model_path: str | Path,
    manifest_path: str | Path,
    dataset_root: str | Path,
    *,
    split: str = "val",
    output_json: str | Path,
    device: str = "cpu",
    conf: float = 0.25,
    max_images: int = 0,
    segmenter: Segmenter | None = None,
) -> Path:
    """Run a segmenter over dataset images and write eval-masks predictions JSON.

    Parameters
    ----------
    model_path / manifest_path:
        ONNX weights and trust manifest (used when ``segmenter`` is None).
    dataset_root:
        YOLO dataset root with ``images/`` (and optionally ``images/{split}/``).
    split:
        Image split name. Prefers ``images/{split}/``; use ``all`` / ``flat`` /
        empty string for a flat ``images/`` directory.
    output_json:
        Destination ``.json`` path (schema v1 predictions cache).
    device:
        Inference device string for :func:`create_segmenter`.
    conf:
        Confidence threshold passed to the segmenter constructor.
    max_images:
        Cap on images to process (0 = unlimited).
    segmenter:
        Optional pre-built segmenter (for tests).
    """
    if isinstance(conf, bool) or not isinstance(conf, (int, float)):
        raise ExportPredictionsError("conf must be a number in [0, 1]")
    conf_value = float(conf)
    if not 0.0 <= conf_value <= 1.0:
        raise ExportPredictionsError("conf must be in [0, 1]")

    output = Path(output_json)
    if output.is_symlink():
        raise ExportPredictionsError("output path must not be a symlink")
    if output.suffix.lower() != ".json":
        raise ExportPredictionsError("output path must end with .json")

    images_dir = resolve_images_dir(dataset_root, split)
    image_paths = collect_image_paths(images_dir, max_images=max_images)
    if not image_paths:
        raise ExportPredictionsError(f"no images found under {images_dir}")

    if segmenter is None:
        model = Path(model_path)
        manifest = Path(manifest_path)
        if model.is_symlink() or not model.is_file():
            raise ExportPredictionsError("model path must be a regular local file")
        if manifest.is_symlink() or not manifest.is_file():
            raise ExportPredictionsError("manifest path must be a regular local file")
        segmenter = create_segmenter(
            model,
            manifest,
            confidence=conf_value,
            device=device,
        )

    entries: list[tuple[str, int, int, tuple[InstanceMask, ...]]] = []
    for frame_index, image_path in enumerate(image_paths):
        try:
            frame = _load_frame_bgr(image_path)
        except Exception as error:
            raise ExportPredictionsError(f"could not load image {image_path}: {error}") from error
        height, width = int(frame.shape[0]), int(frame.shape[1])
        try:
            instances = tuple(segmenter.predict(frame, frame_index=frame_index))
        except Exception as error:
            raise ExportPredictionsError(
                f"segmenter failed on {image_path.name}: {error}"
            ) from error
        entries.append((image_path.stem, width, height, instances))

    payload = predictions_payload_from_instances(entries)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.parent.is_symlink():
        raise ExportPredictionsError("output parent must not be a symlink")
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    output.write_text(text, encoding="utf-8")
    return output


def run_eval_masks_if_available(
    preds_json: str | Path,
    dataset_root: str | Path,
    split: str,
    out_json: str | Path | None = None,
) -> dict[str, Any] | None:
    """Soft-import eval-masks and return metrics, or ``None`` if unavailable.

    Intended as an optional train-auto hook. Import failures yield ``None`` so
    callers can continue without hard-depending on the evaluation package.
    Evaluation/runtime errors propagate to the caller.
    """
    try:
        from cs2_vision_access.workflows.evaluation import (
            evaluate_masks_from_files,
            write_evaluation_json,
        )
    except ImportError:
        return None

    result = evaluate_masks_from_files(
        predictions_path=preds_json,
        dataset_root=dataset_root,
        split=split,
    )
    if out_json is not None:
        write_evaluation_json(result, out_json)
    return result.as_dict()
