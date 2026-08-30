"""Shared dataset layout / label-count helpers for auto-train stages.

Does not import ``stages`` (or prepare/label stage modules) to avoid cycles.
"""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path

from cs2_vision_access.workflows.training.contracts import Layout, write_dataset_yaml
from cs2_vision_access.workflows.training.dataset_zip import count_images, count_labels


def has_split_dataset(root: Path) -> bool:
    return (root / "images" / "train").is_dir() and (root / "labels" / "train").is_dir()


def has_flat_dataset(root: Path) -> bool:
    return (root / "images").is_dir() and (root / "labels").is_dir()


def write_session_dataset_yaml(
    dataset_root: Path,
    class_names: Mapping[int, str],
) -> Path:
    """Write a local-train compatible dataset.yaml under the split dataset root."""
    return write_dataset_yaml(
        dataset_root,
        layout=Layout.SESSION_SPLIT,
        classes=class_names,
        portable_path=False,
    )


def count_images_recursive(images_dir: Path) -> int:
    n = count_images(images_dir)
    if n == 0 and images_dir.is_dir():
        for child in images_dir.iterdir():
            if child.is_dir():
                n += count_images(child)
    return n


def count_labels_recursive(labels_dir: Path) -> int:
    n = count_labels(labels_dir)
    if n == 0 and labels_dir.is_dir():
        for child in labels_dir.iterdir():
            if child.is_dir():
                n += count_labels(child)
    return n


def label_coverage_ok(
    n_labels: int,
    n_images: int,
    *,
    min_labels: int,
    min_label_ratio: float,
) -> bool:
    """True when count and ratio thresholds are both met (images>0 required for ratio)."""
    if n_labels < min_labels:
        return False
    if n_images <= 0:
        return False
    return (n_labels / n_images) >= min_label_ratio


def split_label_stats(root: Path, split: str) -> tuple[int, int]:
    images_dir = root / "images" / split
    labels_dir = root / "labels" / split
    n_images = count_images(images_dir) if images_dir.is_dir() else 0
    n_labels = count_labels(labels_dir) if labels_dir.is_dir() else 0
    return n_labels, n_images


def labels_sufficient(
    root: Path,
    *,
    min_labels: int,
    min_label_ratio: float,
    mode: str,
) -> bool:
    labels_dir = root / "labels"
    if not labels_dir.is_dir():
        return False
    if mode == "session_split" or (root / "images" / "train").is_dir():
        # Session-split: judge train split coverage (val may stay unlabeled).
        n_labels, n_images = split_label_stats(root, "train")
        if n_images > 0:
            return label_coverage_ok(
                n_labels,
                n_images,
                min_labels=min_labels,
                min_label_ratio=min_label_ratio,
            )
        # Fall back to recursive counts when train images are absent.
    n_labels = count_labels_recursive(labels_dir)
    n_images = count_images_recursive(root / "images") if (root / "images").is_dir() else 0
    return label_coverage_ok(
        n_labels,
        n_images,
        min_labels=min_labels,
        min_label_ratio=min_label_ratio,
    )
