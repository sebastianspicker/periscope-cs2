"""Flatten a session_split dataset tree into a flat cloud training bundle.

``cloud.train`` (``training/cloud.py``) requires a flat ``images/`` + ``labels/``
tree: its preflight image/label counts walk each directory non-recursively and
``_ensure_dataset_yaml`` defaults to ``train: images`` / ``val: images``. A
session-split root (``images/train``, ``images/val``, ...) fails that preflight.

This module converts an assembled split tree (as produced by
:func:`cs2_vision_access.dataset_split.assemble_dataset`, i.e. ``images/{split}/
{session}/...`` + ``labels/{split}/{session}/...`` plus ``sessions.json``
provenance) into the flat bundle the cloud pipeline consumes: a flat root with
``images/`` + ``labels/`` + ``dataset.yaml`` and an uploadable zip produced via
:func:`cs2_vision_access.workflows.training.bundle.create_bundle`.
"""

from __future__ import annotations

import shutil
import zipfile
from collections.abc import Mapping
from pathlib import Path

from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.workflows.training.auto.dataset_helpers import has_split_dataset
from cs2_vision_access.workflows.training.bundle import create_bundle
from cs2_vision_access.workflows.training.contracts import Layout, write_dataset_yaml

CLOUD_DATASET_DIR_NAME = "dataset"
CLOUD_DATASET_ZIP_NAME = "dataset.zip"
_SESSIONS_FILENAME = "sessions.json"
_SPLITS = ("train", "val")


def materialize_flat_cloud_bundle(
    split_root: str | Path,
    cloud_dir: str | Path,
    *,
    class_names: Mapping[int, str],
) -> tuple[Path, Path]:
    """Flatten a session-split tree into a flat bundle for cloud training.

    Every labeled image under ``images/train`` and ``images/val`` (with its
    matching label) is copied into ``cloud_dir/dataset/images`` +
    ``cloud_dir/dataset/labels`` under a flattened, collision-free stem
    (``<split>_<session>__<relative_stem>``). A flat ``dataset.yaml`` is
    written, ``sessions.json`` provenance is copied when present, and the tree
    is zipped to ``cloud_dir/dataset.zip`` via
    :func:`cs2_vision_access.workflows.training.bundle.create_bundle`.

    Args:
        split_root: Assembled session-split dataset root (``images/train`` and
            ``labels/train`` must exist).
        cloud_dir: Directory that receives ``dataset/`` and ``dataset.zip``.
        class_names: Class id → name map for the written ``dataset.yaml``.

    Returns:
        ``(flat_root, zip_path)`` — the flat tree consumed by ``cloud.train``
        and the uploadable zip placed under ``cloud_dir``.

    Raises:
        ValueError: When ``split_root`` is not a split dataset, contains no
            labeled train/val pairs, or :func:`create_bundle` rejects the
            flattened tree.
    """
    split_root = Path(split_root)
    cloud_dir = Path(cloud_dir)
    if not has_split_dataset(split_root):
        raise ValueError(
            f"session_split cloud bundle requires images/train and labels/train under {split_root}"
        )

    flat_root = cloud_dir / CLOUD_DATASET_DIR_NAME
    if flat_root.exists():
        shutil.rmtree(flat_root)
    flat_images = flat_root / "images"
    flat_labels = flat_root / "labels"
    flat_images.mkdir(parents=True, exist_ok=True)
    flat_labels.mkdir(parents=True, exist_ok=True)

    _flatten_split_images_labels(split_root, flat_images, flat_labels)
    write_dataset_yaml(
        flat_root,
        layout=Layout.FLAT_BOOTSTRAP,
        classes=class_names,
        portable_path=False,
    )
    sessions_dst = _copy_sessions_json(split_root, flat_root)

    names = [str(class_names[index]) for index in sorted(class_names)]
    zip_path = cloud_dir / CLOUD_DATASET_ZIP_NAME
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    create_bundle(flat_root, zip_path, names=names)
    if sessions_dst is not None:
        with zipfile.ZipFile(zip_path, "a", compression=zipfile.ZIP_DEFLATED) as zf:
            zf.write(sessions_dst, arcname=_SESSIONS_FILENAME)
    return flat_root, zip_path


def _flatten_split_images_labels(
    split_root: Path,
    flat_images: Path,
    flat_labels: Path,
) -> None:
    """Copy labeled train/val pairs into a flat images/ + labels/ tree."""
    copied = 0
    for split in _SPLITS:
        split_images = split_root / "images" / split
        split_labels = split_root / "labels" / split
        if not split_images.is_dir():
            continue
        for image_path in sorted(split_images.rglob("*")):
            if not image_path.is_file() or image_path.suffix.lower() not in IMAGE_EXTENSIONS:
                continue
            relative_stem = image_path.relative_to(split_root / "images").with_suffix("")
            flat_stem = relative_stem.as_posix().replace("/", "_")
            label_path = split_labels / image_path.relative_to(split_images).with_suffix(".txt")
            if not label_path.is_file():
                continue
            shutil.copy2(image_path, flat_images / f"{flat_stem}{image_path.suffix.lower()}")
            shutil.copy2(label_path, flat_labels / f"{flat_stem}.txt")
            copied += 1
    if copied == 0:
        raise ValueError(
            f"no labeled train/val image pairs found under {split_root}; "
            "cannot build a cloud training bundle"
        )


def _copy_sessions_json(split_root: Path, flat_root: Path) -> Path | None:
    """Copy ``sessions.json`` provenance into the flat root when present."""
    source = split_root / _SESSIONS_FILENAME
    if not source.is_file():
        return None
    destination = flat_root / _SESSIONS_FILENAME
    shutil.copy2(source, destination)
    return destination


__all__ = [
    "CLOUD_DATASET_DIR_NAME",
    "CLOUD_DATASET_ZIP_NAME",
    "materialize_flat_cloud_bundle",
]
