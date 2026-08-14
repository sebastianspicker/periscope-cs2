"""COCO person bootstrap helper for the auto-train label stage."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from cs2_vision_access.training.dataset_zip import count_images, count_labels

from .config import AutoTrainConfig
from .dataset_helpers import (
    count_labels_recursive,
    label_coverage_ok,
    labels_sufficient,
    write_session_dataset_yaml,
)
from .errors import AutoTrainStageError
from .notes import append_soft_note


def _run_coco_person_bootstrap(
    config: AutoTrainConfig,
    *,
    root: Path,
    is_session: bool,
    is_flat: bool,
    n_images: int,
    min_labels: int,
    min_label_ratio: float,
    notes: list[str],
    updates: dict[str, Any],
) -> dict[str, Any]:
    """Existing COCO person YOLO-seg bootstrap path."""
    if not is_session and not is_flat:
        updates["label_status"] = "skipped_not_flat"
        notes.append(
            "label bootstrap skipped: dataset is not a flat or session_split "
            f"images/labels layout (images={n_images})"
        )
        updates["label_notes"] = notes
        return updates

    if n_images == 0:
        updates["label_status"] = "skipped_no_images"
        notes.append("label bootstrap skipped: no images")
        updates["label_notes"] = notes
        return updates

    if not config.label.bootstrap:
        updates["label_status"] = "skipped_bootstrap_disabled"
        notes.append(f"labels sparse but label.bootstrap=false (images={n_images})")
        updates["label_notes"] = notes
        if config.label.required:
            raise AutoTrainStageError(
                "label.required=true but labels are sparse and bootstrap is disabled "
                f"({n_images} images)"
            )
        return updates

    try:
        from cs2_vision_access.training.bootstrap_labels import (
            bootstrap_class_id,
            bootstrap_labels_with_yolo_person,
        )
    except ImportError as error:
        updates["label_status"] = "skipped_import"
        notes.append(f"bootstrap import failed (soft): {error}")
        updates["label_notes"] = notes
        append_soft_note(updates, notes[-1])
        if config.label.required:
            raise AutoTrainStageError(
                f"label.required=true but bootstrap import failed: {error}"
            ) from error
        return updates

    base_model = config.train.base_model or "yolo11n-seg.pt"
    cid = bootstrap_class_id(config.train.class_names)
    total_written = 0
    try:
        if is_session:
            splits = tuple(config.label.bootstrap_splits) or ("train",)
            updates["bootstrap_splits"] = list(splits)
            for split in splits:
                split_images = root / "images" / split
                split_labels = root / "labels" / split
                split_n_images = count_images(split_images) if split_images.is_dir() else 0
                split_n_labels = count_labels(split_labels) if split_labels.is_dir() else 0
                if split_n_images == 0:
                    notes.append(f"bootstrap skip split={split}: no images")
                    continue
                if label_coverage_ok(
                    split_n_labels,
                    split_n_images,
                    min_labels=min_labels,
                    min_label_ratio=min_label_ratio,
                ):
                    notes.append(
                        f"bootstrap skip split={split}: already sufficient "
                        f"({split_n_labels}/{split_n_images})"
                    )
                    continue
                written = bootstrap_labels_with_yolo_person(
                    root,
                    device=config.train.device,
                    conf=config.label.conf,
                    base_model=base_model,
                    class_id=cid,
                    images_dir=split_images,
                    labels_dir=split_labels,
                    classes=config.train.class_names,
                    write_yaml=False,
                )
                total_written += int(written)
                notes.append(
                    f"bootstrap split={split}: wrote {written} label file(s) "
                    f"({split_n_labels}/{split_n_images} before)"
                )
            yaml_path = write_session_dataset_yaml(root, config.train.class_names)
            updates["dataset_yaml"] = str(yaml_path.resolve())
        else:
            written = bootstrap_labels_with_yolo_person(
                root,
                device=config.train.device,
                conf=config.label.conf,
                base_model=base_model,
                class_id=cid,
                classes=config.train.class_names,
            )
            total_written = int(written)
            notes.append(f"bootstrap wrote {written} label file(s)")

        n_after = count_labels_recursive(root / "labels")
        updates["label_status"] = "bootstrapped"
        updates["labels_written"] = total_written
        updates["label_count"] = n_after
        notes.append(f"bootstrap total written={total_written}; now {n_after} labels")
    except Exception as error:  # noqa: BLE001 — soft fail unless required
        updates["label_status"] = "bootstrap_failed"
        notes.append(f"bootstrap failed (soft): {error}")
        append_soft_note(updates, notes[-1])
        updates["label_notes"] = notes
        if config.label.required:
            raise AutoTrainStageError(
                f"label.required=true but bootstrap failed: {error}"
            ) from error
        return updates

    still_sparse = not labels_sufficient(
        root,
        min_labels=min_labels,
        min_label_ratio=min_label_ratio,
        mode=config.mode,
    )
    if still_sparse:
        n_after = int(updates.get("label_count") or 0)
        sparse_msg = (
            f"labels remain sparse after bootstrap "
            f"({n_after} labels / {n_images} images; "
            f"min_labels={min_labels}, min_label_ratio={min_label_ratio})"
        )
        notes.append(sparse_msg)
        updates["label_status"] = "bootstrap_insufficient"
        append_soft_note(updates, sparse_msg)
        if config.label.required:
            updates["label_notes"] = notes
            raise AutoTrainStageError("label.required=true but " + sparse_msg)

    updates["label_notes"] = notes
    return updates
