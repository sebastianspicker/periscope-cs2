"""Layout detection and image/label pair discovery for box-dataset import."""

from __future__ import annotations

import os
from pathlib import Path

from cs2_vision_access.workflows.dataset._fs import files_under
from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS, SPLIT_NAMES

LAYOUT_AUTO = "auto"
LAYOUT_YOLO = "yolo"
LAYOUT_FLAT = "flat"
LAYOUT_PAIRS = "pairs"
SUPPORTED_LAYOUTS = frozenset({LAYOUT_AUTO, LAYOUT_YOLO, LAYOUT_FLAT})
RESOLVED_LAYOUTS = frozenset({LAYOUT_YOLO, LAYOUT_FLAT, LAYOUT_PAIRS})
DEFAULT_SPLIT = "train"


class ImportBoxDatasetError(ValueError):
    """Source layout or staging destination cannot be imported safely."""


def detect_layout(source_root: str | Path) -> str:
    """Resolve ``auto`` layout against an existing source directory."""
    source = require_existing_dir(source_root, "source root")
    images = source / "images"
    labels = source / "labels"
    if images.is_dir() and not images.is_symlink() and labels.is_dir() and not labels.is_symlink():
        split_dirs = [
            name
            for name in sorted(SPLIT_NAMES)
            if (images / name).is_dir()
            and not (images / name).is_symlink()
            and (labels / name).is_dir()
            and not (labels / name).is_symlink()
        ]
        if split_dirs:
            return LAYOUT_YOLO
        # Flat images/ + labels/ without train|val|test children.
        return LAYOUT_FLAT

    # Same-stem pairs in the source root (or one-level children).
    pair_images, _ = files_under(source, IMAGE_EXTENSIONS)
    # Only consider files that are not under a YOLO images/ tree we already rejected.
    pair_images = [
        path for path in pair_images if "images" not in path.relative_to(source).parts[:1]
    ]
    if pair_images:
        return LAYOUT_PAIRS
    raise ImportBoxDatasetError(
        "could not detect YOLO det layout; expected images/{split}+labels/{split}, "
        "images/+labels/, or same-stem image+label pairs"
    )


def collect_detection_pairs(
    source_root: str | Path,
    *,
    layout: str,
    split: str = DEFAULT_SPLIT,
) -> list[tuple[Path, Path, str]]:
    """Return ``(image, label, relative_stem)`` sorted by stem (fail closed on orphans)."""
    source = require_existing_dir(source_root, "source root")
    if layout == LAYOUT_YOLO:
        return _pairs_from_yolo_split(source, split)
    if layout == LAYOUT_FLAT:
        return _pairs_from_trees(source / "images", source / "labels", "flat")
    if layout == LAYOUT_PAIRS:
        return _pairs_from_flat_directory(source)
    raise ImportBoxDatasetError(f"unknown resolved layout: {layout!r}")


def require_existing_dir(path: str | Path, label: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise ImportBoxDatasetError(f"{label} must not be a symlink")
    if not candidate.is_dir():
        raise ImportBoxDatasetError(f"{label} is not a directory: {candidate}")
    return candidate


def normalise_split(split: str) -> str | None:
    if not isinstance(split, str) or not split.strip():
        return None
    name = split.strip().lower()
    if name not in SPLIT_NAMES:
        raise ImportBoxDatasetError(f"split must be one of {sorted(SPLIT_NAMES)}; got {split!r}")
    return name


def _pairs_from_yolo_split(source: Path, split: str) -> list[tuple[Path, Path, str]]:
    split_name = normalise_split(split)
    if split_name is None:
        raise ImportBoxDatasetError("split must be a non-empty string")
    images_dir = source / "images" / split_name
    labels_dir = source / "labels" / split_name
    if images_dir.is_symlink() or labels_dir.is_symlink():
        raise ImportBoxDatasetError(
            f"images/{split_name} and labels/{split_name} must not be symlinks"
        )
    if not images_dir.is_dir():
        raise ImportBoxDatasetError(f"yolo layout missing images/{split_name}: {images_dir}")
    if not labels_dir.is_dir():
        raise ImportBoxDatasetError(f"yolo layout missing labels/{split_name}: {labels_dir}")
    return _pairs_from_trees(images_dir, labels_dir, f"yolo/{split_name}")


def _pairs_from_trees(
    images_dir: Path, labels_dir: Path, context: str
) -> list[tuple[Path, Path, str]]:
    if images_dir.is_symlink() or labels_dir.is_symlink():
        raise ImportBoxDatasetError(f"{context}: images/ and labels/ must not be symlinks")
    if not images_dir.is_dir():
        raise ImportBoxDatasetError(f"{context}: images directory missing: {images_dir}")
    if not labels_dir.is_dir():
        raise ImportBoxDatasetError(f"{context}: labels directory missing: {labels_dir}")

    image_files, image_symlinks = files_under(images_dir, IMAGE_EXTENSIONS)
    if image_symlinks:
        raise ImportBoxDatasetError(
            f"{context}: symlink under images is not allowed: {image_symlinks[0]}"
        )
    if not image_files:
        raise ImportBoxDatasetError(f"{context}: no supported images under {images_dir}")

    label_files, label_symlinks = files_under(labels_dir, frozenset({".txt"}))
    if label_symlinks:
        raise ImportBoxDatasetError(
            f"{context}: symlink under labels is not allowed: {label_symlinks[0]}"
        )
    labels_by_stem = {
        path.relative_to(labels_dir).with_suffix("").as_posix(): path for path in label_files
    }

    pairs: list[tuple[Path, Path, str]] = []
    seen: set[str] = set()
    for image_path in image_files:
        relative_stem = image_path.relative_to(images_dir).with_suffix("").as_posix()
        label_path = labels_by_stem.get(relative_stem)
        if label_path is None:
            raise ImportBoxDatasetError(
                f"{context}: image missing same-stem label: {relative_stem}"
            )
        pairs.append((image_path, label_path, relative_stem))
        seen.add(relative_stem)

    orphans = sorted(set(labels_by_stem) - seen)
    if orphans:
        raise ImportBoxDatasetError(
            f"{context}: orphan labels without images: {', '.join(orphans[:5])}"
        )
    return pairs


def _pairs_from_flat_directory(source: Path) -> list[tuple[Path, Path, str]]:
    image_files, image_symlinks = files_under(source, IMAGE_EXTENSIONS)
    image_files = [
        path
        for path in image_files
        if "images" not in path.relative_to(source).parts[:1]
        and "labels" not in path.relative_to(source).parts[:1]
    ]
    if image_symlinks:
        # Still reject symlinks that walk would have collected under source.
        raise ImportBoxDatasetError(f"symlink under source is not allowed: {image_symlinks[0]}")
    if not image_files:
        raise ImportBoxDatasetError(f"pairs layout: no supported images under {source}")

    pairs: list[tuple[Path, Path, str]] = []
    image_stems: set[str] = set()
    for image_path in image_files:
        relative = image_path.relative_to(source)
        relative_stem = relative.with_suffix("").as_posix()
        label_path = image_path.with_suffix(".txt")
        if label_path.is_symlink():
            raise ImportBoxDatasetError(
                f"pairs layout: label must not be a symlink: {relative_stem}"
            )
        if not label_path.is_file():
            raise ImportBoxDatasetError(
                f"pairs layout: image missing same-stem label: {relative.as_posix()}"
            )
        pairs.append((image_path, label_path, Path(relative_stem).name))
        image_stems.add(relative_stem)

    # Orphan .txt beside images (same directory walk, no follow).
    _, _ = files_under(source, frozenset({".txt"}))
    for current_root, directory_names, filenames in os.walk(source, followlinks=False):
        current = Path(current_root)
        directory_names[:] = [name for name in directory_names if not (current / name).is_symlink()]
        # Skip YOLO tree names if present.
        if current == source:
            directory_names[:] = [
                name for name in directory_names if name not in {"images", "labels"}
            ]
        for name in filenames:
            candidate = current / name
            if candidate.is_symlink() or candidate.suffix.lower() != ".txt":
                continue
            stem = candidate.relative_to(source).with_suffix("").as_posix()
            if stem not in image_stems:
                raise ImportBoxDatasetError(f"pairs layout: orphan label without image: {stem}")
    return pairs
