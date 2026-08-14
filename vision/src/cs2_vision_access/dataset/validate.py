"""Validate Ultralytics YOLO instance-segmentation dataset trees."""

from __future__ import annotations

import math
from decimal import Decimal
from pathlib import Path

from cs2_vision_access.dataset._fs import files_under, relative
from cs2_vision_access.dataset.types import (
    IMAGE_EXTENSIONS,
    OPTIONAL_SPLITS,
    REQUIRED_SPLITS,
    DatasetIssue,
    DatasetSummary,
    DatasetValidationResult,
)


def validate_yolo_segmentation_dataset(
    dataset_root: str | Path, class_count: int
) -> DatasetValidationResult:
    """Validate an Ultralytics-style YOLO segmentation dataset.

    The expected tree is ``images/{train,val,test?}`` and a matching ``labels``
    tree.  Empty label files represent valid negative images.  The result is
    deterministic: paths, file traversal, and issues are sorted lexically.
    """
    root = Path(dataset_root)
    issues: list[DatasetIssue] = []
    image_count = label_count = negative_label_count = annotation_count = 0

    if not isinstance(class_count, int) or isinstance(class_count, bool) or class_count <= 0:
        issues.append(
            DatasetIssue(
                "INVALID_CLASS_COUNT",
                ".",
                "class_count must be a positive integer",
            )
        )
        return _result(issues, image_count, label_count, negative_label_count, annotation_count)

    if root.is_symlink():
        issues.append(DatasetIssue("SYMLINKED_ROOT", ".", "dataset root must not be a symlink"))
        return _result(issues, image_count, label_count, negative_label_count, annotation_count)
    if not root.is_dir():
        issues.append(DatasetIssue("MISSING_ROOT", ".", "dataset root is not a directory"))
        return _result(issues, image_count, label_count, negative_label_count, annotation_count)

    images_root = root / "images"
    labels_root = root / "labels"
    required_directories = (
        (images_root, "MISSING_IMAGES_DIRECTORY"),
        (labels_root, "MISSING_LABELS_DIRECTORY"),
    )
    for directory, code in required_directories:
        if directory.is_symlink():
            issues.append(
                DatasetIssue(
                    "SYMLINKED_PATH",
                    relative(root, directory),
                    "symlinks are not allowed",
                )
            )
        elif not directory.is_dir():
            issues.append(
                DatasetIssue(
                    code,
                    relative(root, directory),
                    "required directory is missing",
                )
            )

    for split in REQUIRED_SPLITS + OPTIONAL_SPLITS:
        images_split = images_root / split
        labels_split = labels_root / split
        image_split_exists = images_split.is_dir() and not images_split.is_symlink()
        label_split_exists = labels_split.is_dir() and not labels_split.is_symlink()
        if split in REQUIRED_SPLITS:
            if images_split.is_symlink():
                issues.append(
                    DatasetIssue(
                        "SYMLINKED_PATH",
                        relative(root, images_split),
                        "symlinks are not allowed",
                    )
                )
            elif not images_split.is_dir():
                issues.append(
                    DatasetIssue(
                        "MISSING_IMAGE_SPLIT",
                        relative(root, images_split),
                        "required image split is missing",
                    )
                )
            if labels_split.is_symlink():
                issues.append(
                    DatasetIssue(
                        "SYMLINKED_PATH",
                        relative(root, labels_split),
                        "symlinks are not allowed",
                    )
                )
            elif not labels_split.is_dir():
                issues.append(
                    DatasetIssue(
                        "MISSING_LABEL_SPLIT",
                        relative(root, labels_split),
                        "required label split is missing",
                    )
                )
        else:
            if images_split.is_symlink():
                issues.append(
                    DatasetIssue(
                        "SYMLINKED_PATH",
                        relative(root, images_split),
                        "symlinks are not allowed",
                    )
                )
            if labels_split.is_symlink():
                issues.append(
                    DatasetIssue(
                        "SYMLINKED_PATH",
                        relative(root, labels_split),
                        "symlinks are not allowed",
                    )
                )
            if (
                images_split.exists()
                and not images_split.is_dir()
                and not images_split.is_symlink()
            ):
                issues.append(
                    DatasetIssue(
                        "INVALID_SPLIT_PATH",
                        relative(root, images_split),
                        "optional split path must be a directory",
                    )
                )
            if (
                labels_split.exists()
                and not labels_split.is_dir()
                and not labels_split.is_symlink()
            ):
                issues.append(
                    DatasetIssue(
                        "INVALID_SPLIT_PATH",
                        relative(root, labels_split),
                        "optional split path must be a directory",
                    )
                )
            if images_split.exists() != labels_split.exists():
                existing = images_split if images_split.exists() else labels_split
                issues.append(
                    DatasetIssue(
                        "UNPAIRED_SPLIT",
                        relative(root, existing),
                        "image and label splits must both exist",
                    )
                )

        if not image_split_exists or not label_split_exists:
            continue

        image_files, image_symlinks = files_under(images_split, IMAGE_EXTENSIONS)
        label_files, label_symlinks = files_under(labels_split, frozenset({".txt"}))
        if split in REQUIRED_SPLITS and not image_files:
            issues.append(
                DatasetIssue(
                    "EMPTY_IMAGE_SPLIT",
                    relative(root, images_split),
                    "required image split must contain at least one supported image",
                )
            )
        label_count += len(label_files)
        for path in image_symlinks + label_symlinks:
            issues.append(
                DatasetIssue(
                    "SYMLINKED_PATH",
                    relative(root, path),
                    "symlinks are not allowed",
                )
            )

        labels_by_relative = {
            path.relative_to(labels_split).with_suffix(""): path for path in label_files
        }
        image_keys: set[Path] = set()
        for image_path in image_files:
            image_count += 1
            key = image_path.relative_to(images_split).with_suffix("")
            image_keys.add(key)
            label_path = labels_by_relative.get(key)
            if label_path is None:
                issues.append(
                    DatasetIssue(
                        "MISSING_LABEL",
                        relative(root, image_path),
                        "image has no corresponding .txt label",
                    )
                )
                continue
            rows, row_issues = _validate_label(label_path, root, class_count)
            annotation_count += rows
            if rows == 0 and not row_issues:
                negative_label_count += 1
            issues.extend(row_issues)

        sorted_labels = sorted(
            labels_by_relative.items(),
            key=lambda entry: entry[0].as_posix(),
        )
        for key, label_path in sorted_labels:
            if key not in image_keys:
                issues.append(
                    DatasetIssue(
                        "ORPHAN_LABEL",
                        relative(root, label_path),
                        "label has no corresponding image",
                    )
                )

    issues.sort(
        key=lambda issue: (
            issue.path,
            issue.line is None,
            issue.line or 0,
            issue.code,
            issue.message,
        )
    )
    return _result(issues, image_count, label_count, negative_label_count, annotation_count)


def _validate_label(
    label_path: Path, root: Path, class_count: int
) -> tuple[int, list[DatasetIssue]]:
    issues: list[DatasetIssue] = []
    valid_rows = 0
    try:
        contents = label_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        return 0, [DatasetIssue("UNREADABLE_LABEL", relative(root, label_path), str(error))]

    for line_number, row in enumerate(contents.splitlines(), start=1):
        tokens = row.split()
        if not tokens:
            continue
        if len(tokens) < 7 or (len(tokens) - 1) % 2:
            issues.append(
                DatasetIssue(
                    "MALFORMED_POLYGON",
                    relative(root, label_path),
                    "row needs a class id and at least three coordinate pairs",
                    line_number,
                )
            )
            continue
        if not _is_class_id(tokens[0], class_count):
            issues.append(
                DatasetIssue(
                    "INVALID_CLASS_ID",
                    relative(root, label_path),
                    "class id is outside the configured range",
                    line_number,
                )
            )
            continue
        try:
            coordinates = [float(token) for token in tokens[1:]]
        except ValueError:
            issues.append(
                DatasetIssue(
                    "INVALID_COORDINATE",
                    relative(root, label_path),
                    "coordinates must be finite decimal numbers",
                    line_number,
                )
            )
            continue
        if any(not math.isfinite(value) or not 0.0 <= value <= 1.0 for value in coordinates):
            issues.append(
                DatasetIssue(
                    "COORDINATE_OUT_OF_RANGE",
                    relative(root, label_path),
                    "coordinates must be finite values in [0, 1]",
                    line_number,
                )
            )
            continue
        if _polygon_area([Decimal(token) for token in tokens[1:]]) == 0:
            issues.append(
                DatasetIssue(
                    "DEGENERATE_POLYGON",
                    relative(root, label_path),
                    "polygon area must be nonzero",
                    line_number,
                )
            )
            continue
        valid_rows += 1
    return valid_rows, issues


def _is_class_id(value: str, class_count: int) -> bool:
    return value.isascii() and value.isdecimal() and int(value) < class_count


def _polygon_area(coordinates: list[Decimal]) -> Decimal:
    points = list(zip(coordinates[::2], coordinates[1::2], strict=False))
    rolled = points[1:] + points[:1]
    cross_products = (x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(points, rolled, strict=False))
    return abs(sum(cross_products, Decimal(0))) / 2


def _result(
    issues: list[DatasetIssue],
    image_count: int,
    label_count: int,
    negative_label_count: int,
    annotation_count: int,
) -> DatasetValidationResult:
    return DatasetValidationResult(
        tuple(issues),
        DatasetSummary(
            image_count,
            label_count,
            negative_label_count,
            annotation_count,
            len(issues),
        ),
    )
