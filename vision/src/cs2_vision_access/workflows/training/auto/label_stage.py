"""Label bootstrap stage for auto-train pipelines.

Extracted from ``stages.py`` to keep stage runners modular.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from .config import AutoTrainConfig
from .dataset_helpers import (
    count_images_recursive,
    count_labels_recursive,
    has_flat_dataset,
    labels_sufficient,
)
from .errors import AutoTrainStageError
from .label_coco_bootstrap import (
    _run_coco_person_bootstrap,
)
from .label_edgesam import (
    _run_edgesam_teacher,
)
from .paths import RunPaths
from .state import StageState


def stage_label(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Bootstrap or skip labels before validate.

    No-op when labels already meet ``min_labels`` / ``min_label_ratio``.
    Teachers:
    - ``none``: skip bootstrap
    - ``coco_person``: YOLO-seg COCO person (default)
    - ``edgesam``: Vombit + EdgeSAM via :mod:`prepare_lib` (optional coco fallback)

    Soft on import/runtime failure unless ``label.required``.
    """
    notes: list[str] = []
    updates: dict[str, Any] = {
        "label_enabled": config.label.enabled,
        "label_teacher": config.label.teacher,
    }

    if not config.label.enabled:
        updates["label_status"] = "disabled"
        notes.append("label stage disabled")
        updates["label_notes"] = notes
        return updates

    dataset_root = state.artifacts.get("dataset_root")
    if not dataset_root:
        updates["label_status"] = "skipped_no_dataset"
        notes.append("label skipped: no dataset_root")
        updates["label_notes"] = notes
        return updates

    root = Path(str(dataset_root))
    if not root.is_dir():
        raise AutoTrainStageError(f"label: dataset_root is not a directory: {root}")

    min_labels = max(1, config.train.min_labels)
    min_label_ratio = float(config.label.min_label_ratio)
    if labels_sufficient(
        root,
        min_labels=min_labels,
        min_label_ratio=min_label_ratio,
        mode=config.mode,
    ):
        n_labels = count_labels_recursive(root / "labels")
        updates["label_status"] = "sufficient"
        updates["label_count"] = n_labels
        notes.append(f"labels already sufficient ({n_labels} files)")
        updates["label_notes"] = notes
        return updates

    images_dir = root / "images"
    labels_dir = root / "labels"
    n_images = count_images_recursive(images_dir) if images_dir.is_dir() else 0
    n_labels = count_labels_recursive(labels_dir) if labels_dir.is_dir() else 0
    updates["label_count_before"] = n_labels
    updates["image_count"] = n_images

    is_session = (root / "images" / "train").is_dir()
    is_flat = has_flat_dataset(root) and not is_session

    teacher = str(config.label.teacher or "coco_person").strip().lower()

    if teacher == "none":
        updates["label_status"] = "skipped_bootstrap_disabled"
        notes.append(
            f"labels sparse ({n_labels}/{n_images}) but label.teacher=none (bootstrap disabled)"
        )
        updates["label_notes"] = notes
        if config.label.required:
            raise AutoTrainStageError(
                "label.required=true but labels are sparse and teacher=none "
                f"({n_labels} labels / {n_images} images)"
            )
        return updates

    has_images_layout = is_session or is_flat

    if not has_images_layout and teacher != "edgesam":
        updates["label_status"] = "skipped_not_flat"
        notes.append(
            "label bootstrap skipped: dataset is not a flat or session_split "
            f"images/labels layout (images={n_images}, labels={n_labels})"
        )
        updates["label_notes"] = notes
        return updates

    if n_images == 0 and teacher != "edgesam":
        updates["label_status"] = "skipped_no_images"
        notes.append("label bootstrap skipped: no images")
        updates["label_notes"] = notes
        return updates

    if not config.label.bootstrap and teacher == "coco_person":
        updates["label_status"] = "skipped_bootstrap_disabled"
        notes.append(f"labels sparse ({n_labels}/{n_images}) but label.bootstrap=false")
        updates["label_notes"] = notes
        if config.label.required:
            raise AutoTrainStageError(
                "label.required=true but labels are sparse and bootstrap is disabled "
                f"({n_labels} labels / {n_images} images)"
            )
        return updates

    if teacher == "edgesam":
        edgesam_updates = _run_edgesam_teacher(
            config,
            paths,
            state,
            root=root,
            is_session=is_session,
            is_flat=is_flat,
            n_images=n_images,
            n_labels=n_labels,
            min_labels=min_labels,
            min_label_ratio=min_label_ratio,
            notes=notes,
            updates=updates,
        )
        if edgesam_updates is not None:
            return edgesam_updates
        # Fall through to coco when coco_fallback allowed and edgesam soft-failed.

    return _run_coco_person_bootstrap(
        config,
        root=root,
        is_session=is_session,
        is_flat=is_flat,
        n_images=n_images,
        min_labels=min_labels,
        min_label_ratio=min_label_ratio,
        notes=notes,
        updates=updates,
    )
