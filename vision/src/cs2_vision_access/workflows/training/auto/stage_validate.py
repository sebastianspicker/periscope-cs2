"""Validate stage for auto-train pipelines."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.dataset import validate_yolo_segmentation_dataset
from cs2_vision_access.workflows.training.contracts import assert_label_class_ids_compatible_tree
from cs2_vision_access.workflows.training.progress_report import analyze_labels

from .config import AutoTrainConfig
from .dataset_helpers import (
    count_images_recursive,
    count_labels_recursive,
)
from .errors import AutoTrainStageError
from .notes import append_soft_note
from .paths import RunPaths
from .state import StageState


def _read_yaml_train_val(dataset_yaml: Path) -> tuple[str | None, str | None]:
    try:
        import yaml
    except ImportError:
        return None, None
    try:
        payload = yaml.safe_load(dataset_yaml.read_text(encoding="utf-8"))
    except Exception:
        return None, None
    if not isinstance(payload, dict):
        return None, None
    train = payload.get("train")
    val = payload.get("val")
    return (
        str(train) if isinstance(train, str) else None,
        str(val) if isinstance(val, str) else None,
    )


def _enforce_label_class_ids(
    labels_dir: Path,
    config: AutoTrainConfig,
    updates: dict[str, Any],
) -> None:
    """Hard-fail when label class ids are outside config.train.class_names."""
    if not labels_dir.is_dir():
        return
    try:
        warnings = assert_label_class_ids_compatible_tree(labels_dir, config.train.class_names)
    except ValueError as error:
        raise AutoTrainStageError(str(error)) from error
    if warnings:
        updates["label_class_id_warnings"] = warnings


def stage_validate(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Validate dataset structure / soft label counts before train."""
    del paths  # unused; kept for stage signature symmetry
    dataset_root = state.artifacts.get("dataset_root")
    if not dataset_root:
        raise AutoTrainStageError("validate requires dataset_root from prepare_data")
    root = Path(str(dataset_root))
    if not root.is_dir():
        raise AutoTrainStageError(f"dataset_root is not a directory: {root}")

    class_count = len(config.train.class_names)
    updates: dict[str, Any] = {"dataset_root": str(root.resolve())}

    # Health summary via analyze_labels (soft).
    labels_dir = root / "labels"
    try:
        analysis = analyze_labels(labels_dir, class_names=config.train.class_names)
        empty_ratio = (analysis.n_empty / analysis.n_files) if analysis.n_files > 0 else 0.0
        health = {
            "n_files": analysis.n_files,
            "n_empty": analysis.n_empty,
            "empty_ratio": empty_ratio,
            "class_counts": {str(k): v for k, v in analysis.class_counts.items()},
        }
        updates["label_health"] = health
        # Soft warning when many empty labels (background-only frames).
        if analysis.n_files > 0 and empty_ratio >= 0.9:
            updates["empty_label_ratio_warning"] = True
            updates["label_health_warning"] = (
                f"empty label ratio high: {empty_ratio:.2%} ({analysis.n_empty}/{analysis.n_files})"
            )
            append_soft_note(updates, updates["label_health_warning"])
    except Exception as error:  # noqa: BLE001
        updates["label_health_error"] = str(error)

    if config.mode == "session_split":
        result = validate_yolo_segmentation_dataset(root, class_count)
        updates["validation"] = {
            "is_valid": result.is_valid,
            "issue_count": result.summary.issue_count,
            "image_count": result.summary.image_count,
            "label_count": result.summary.label_count,
            "issues": [
                {
                    "code": issue.code,
                    "path": issue.path,
                    "message": issue.message,
                }
                for issue in result.issues[:20]
            ],
        }
        if not result.is_valid:
            first = result.issues[0]
            raise AutoTrainStageError(
                f"dataset validation failed ({result.summary.issue_count} issue(s)); "
                f"first={first.code}:{first.path}:{first.message}"
            )
        _enforce_label_class_ids(labels_dir, config, updates)
        return updates

    # flat_cloud — soft validate: min labels/images; leaky train=val check
    images_dir = root / "images"
    n_images = count_images_recursive(images_dir)
    n_labels = count_labels_recursive(labels_dir)

    updates["validation"] = {
        "image_count": n_images,
        "label_count": n_labels,
        "min_labels": config.train.min_labels,
    }
    if n_labels < config.train.min_labels:
        raise AutoTrainStageError(
            f"flat_cloud soft validate: need at least {config.train.min_labels} "
            f"label file(s), found {n_labels} under {labels_dir}"
        )
    if n_images == 0:
        raise AutoTrainStageError(f"flat_cloud soft validate: no images under {images_dir}")

    yaml_path = root / "dataset.yaml"
    train_split: str | None = None
    val_split: str | None = None
    if yaml_path.is_file():
        train_split, val_split = _read_yaml_train_val(yaml_path)
    # Cloud default format uses train: images / val: images (leaky).
    if train_split is None and val_split is None:
        # Implicit flat layout used by cloud.train → train=val=images
        train_split, val_split = "images", "images"

    is_leaky = (
        train_split is not None
        and val_split is not None
        and Path(train_split).as_posix().rstrip("/") == Path(val_split).as_posix().rstrip("/")
    )
    updates["train_split"] = train_split
    updates["val_split"] = val_split
    updates["leaky_val"] = is_leaky
    if is_leaky and not config.train.allow_leaky_val:
        raise AutoTrainStageError(
            "flat_cloud refuses train=val (leaky validation). "
            "Set train.allow_leaky_val=true to acknowledge, or provide "
            "a dataset.yaml with disjoint train/val paths."
        )
    _enforce_label_class_ids(labels_dir, config, updates)
    return updates
