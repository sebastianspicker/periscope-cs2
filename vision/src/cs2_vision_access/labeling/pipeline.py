"""Main boxes-to-masks orchestration for bootstrap."""

from __future__ import annotations

import json
from pathlib import Path

from cs2_vision_access.labeling.convert import (
    box_to_ellipse_polygon,
    box_to_rectangle_polygon,
    format_segmentation_row,
    parse_detection_label,
)
from cs2_vision_access.labeling.io import (
    collect_images,
    collect_labels,
    require_existing_dir,
    write_text_atomic,
)
from cs2_vision_access.labeling.sam import polygon_for_box_sam
from cs2_vision_access.labeling.types import (
    DEFAULT_BACKEND,
    DRAFT_STATUS_FILENAME,
    REVIEW_STATUS_DRAFT_PENDING,
    SCHEMA_VERSION,
    BootstrapError,
    BootstrapSummary,
    ClassMap,
    DetectionBox,
    parse_backend,
    parse_class_map,
)


def boxes_to_masks(
    images_dir: str | Path,
    labels_dir: str | Path,
    output_labels_dir: str | Path,
    *,
    backend: str = DEFAULT_BACKEND,
    class_map: ClassMap | str | None = None,
    overwrite: bool = False,
) -> BootstrapSummary:
    """Convert detection labels beside images into draft YOLO-seg polygons.

    Writes one same-stem ``.txt`` under ``output_labels_dir`` (mirroring nested
    relative paths) and ``draft_status.json`` with ``review_status`` always
    ``draft_pending``.
    """
    backend_name = parse_backend(backend)
    resolved_map = class_map if isinstance(class_map, ClassMap) else parse_class_map(class_map)

    images_root = require_existing_dir(images_dir, "images directory")
    labels_root = require_existing_dir(labels_dir, "labels directory")
    output_root = Path(output_labels_dir)
    if output_root.is_symlink():
        raise BootstrapError("output labels directory must not be a symlink")
    if output_root.exists() and not output_root.is_dir():
        raise BootstrapError(f"output labels path exists and is not a directory: {output_root}")

    image_files = collect_images(images_root)
    if not image_files:
        raise BootstrapError(f"images directory has no supported images: {images_root}")

    label_files = collect_labels(labels_root)
    labels_by_key = {path.relative_to(labels_root).with_suffix(""): path for path in label_files}
    image_keys = {path.relative_to(images_root).with_suffix("") for path in image_files}

    for key, label_path in sorted(labels_by_key.items(), key=lambda item: item[0].as_posix()):
        if key not in image_keys:
            raise BootstrapError(f"orphan detection label has no image: {label_path}")

    # Fail closed before writing: parse every label and plan every output path.
    planned: list[tuple[Path, Path, Path, list[str]]] = []
    instance_count = 0
    negative_count = 0
    file_records: list[dict[str, object]] = []

    for image_path in image_files:
        key = image_path.relative_to(images_root).with_suffix("")
        label_path = labels_by_key.get(key)
        if label_path is None:
            raise BootstrapError(f"image has no corresponding detection label: {image_path}")
        output_path = output_root / key.with_suffix(".txt")
        boxes = parse_detection_label(label_path)
        rows: list[str] = []
        for box in boxes:
            output_class = resolved_map.resolve(box.source_class_id)
            polygon = _polygon_for_box(
                backend_name,
                box,
                image_path=image_path,
            )
            rows.append(format_segmentation_row(output_class, polygon))
        if rows:
            instance_count += len(rows)
        else:
            negative_count += 1
        planned.append((image_path, label_path, output_path, rows))
        file_records.append(
            {
                "image": image_path.relative_to(images_root).as_posix(),
                "source_label": label_path.relative_to(labels_root).as_posix(),
                "output_label": key.with_suffix(".txt").as_posix(),
                "instance_count": len(rows),
                # Per-file status; SAM/bootstrap drafts never auto-GT.
                "review_status": REVIEW_STATUS_DRAFT_PENDING,
            }
        )

    draft_path = output_root / DRAFT_STATUS_FILENAME
    if not overwrite:
        conflicts = [path for _image, _label, path, _rows in planned if path.exists()]
        if draft_path.exists():
            conflicts.append(draft_path)
        if conflicts:
            sample = ", ".join(path.as_posix() for path in conflicts[:3])
            extra = f" (+{len(conflicts) - 3} more)" if len(conflicts) > 3 else ""
            raise BootstrapError(
                f"output already exists (pass --overwrite to replace): {sample}{extra}"
            )
    else:
        for path in [path for _i, _l, path, _r in planned if path.exists()]:
            if path.is_symlink():
                raise BootstrapError(f"output label must not be a symlink: {path}")
        if draft_path.is_symlink():
            raise BootstrapError("draft_status.json must not be a symlink")

    output_root.mkdir(parents=True, exist_ok=True)
    if output_root.is_symlink():
        raise BootstrapError("output labels directory must not be a symlink")

    for _image_path, _label_path, output_path, rows in planned:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        if output_path.is_symlink():
            raise BootstrapError(f"output label must not be a symlink: {output_path}")
        payload = "\n".join(rows) + ("\n" if rows else "")
        write_text_atomic(output_path, payload)

    draft_payload = {
        "schema_version": SCHEMA_VERSION,
        "review_status": REVIEW_STATUS_DRAFT_PENDING,
        "backend": backend_name,
        "class_map": resolved_map.as_json(),
        "images_dir": str(images_root),
        "labels_dir": str(labels_root),
        "output_labels_dir": str(output_root.resolve()),
        "image_count": len(image_files),
        "label_count": len(planned),
        "instance_count": instance_count,
        "negative_count": negative_count,
        "files": file_records,
    }
    # Never auto-promote: review_status is always draft_pending.
    if draft_payload["review_status"] != REVIEW_STATUS_DRAFT_PENDING:
        raise BootstrapError("internal error: review_status must be draft_pending")
    write_text_atomic(
        draft_path,
        json.dumps(draft_payload, indent=2, sort_keys=True) + "\n",
    )

    return BootstrapSummary(
        images_dir=str(images_root),
        labels_dir=str(labels_root),
        output_labels_dir=str(output_root.resolve()),
        backend=backend_name,
        image_count=len(image_files),
        label_count=len(planned),
        instance_count=instance_count,
        negative_count=negative_count,
        draft_status_path=str(draft_path.resolve()),
        review_status=REVIEW_STATUS_DRAFT_PENDING,
    )


def _polygon_for_box(
    backend: str,
    box: DetectionBox,
    *,
    image_path: Path,
) -> list[float]:
    if backend == "rectangle":
        return box_to_rectangle_polygon(box.x_center, box.y_center, box.width, box.height)
    if backend == "ellipse":
        return box_to_ellipse_polygon(box.x_center, box.y_center, box.width, box.height)
    if backend == "sam":
        return polygon_for_box_sam(box, image_path=image_path)
    raise BootstrapError(f"unknown backend {backend!r}")
