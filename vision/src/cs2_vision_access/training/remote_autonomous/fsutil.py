"""Filesystem helpers and small pure utilities for autonomous training."""

from __future__ import annotations

import os
import shutil
from collections.abc import Mapping
from pathlib import Path

from . import deps


def _auto_device() -> str:
    return deps.auto_train_device()


def _ensure_dirs(data_dir: Path) -> tuple[Path, Path]:
    images = data_dir / "images"
    labels = data_dir / "labels"
    images.mkdir(parents=True, exist_ok=True)
    labels.mkdir(parents=True, exist_ok=True)
    return images, labels


def _list_image_stems(images_dir: Path) -> list[str]:
    from cs2_vision_access.dataset.types import IMAGE_EXTENSIONS

    stems: list[str] = []
    if not images_dir.is_dir():
        return stems
    for path in sorted(images_dir.iterdir()):
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            stems.append(path.stem)
    return stems


def _find_image_path(images_dir: Path, stem: str) -> Path | None:
    return deps.find_image_for_stem(images_dir, stem)


def _link_or_copy(src: Path, dst: Path) -> None:
    """Prefer hardlink; fall back to copy2. No-op if *dst* already exists."""
    if not src.is_file():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        return
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def _extract_map_metric(metrics: Mapping[str, float]) -> tuple[float | None, float | None]:
    """Return (primary_map, mAP50-95) preferring mAP50 then mAP50-95 as primary."""
    map50 = metrics.get("mAP50")
    map50_95 = metrics.get("mAP50-95")
    primary: float | None
    if map50 is not None:
        primary = float(map50)
    elif map50_95 is not None:
        primary = float(map50_95)
    else:
        primary = None
    return primary, (float(map50_95) if map50_95 is not None else None)


def _labeled_count(labels_dir: Path) -> int:
    if not labels_dir.is_dir():
        return 0
    return sum(1 for p in labels_dir.iterdir() if p.is_file() and p.suffix.lower() == ".txt")


def _is_oom_error(exc: BaseException) -> bool:
    return deps.is_oom_error(exc)


def _assert_labels_compatible(labels_dirs: list[Path], class_map: Mapping[int, str]) -> list[str]:
    warnings: list[str] = []
    for labels_dir in labels_dirs:
        if not labels_dir.is_dir():
            continue
        # Skip dirs with no label files (e.g. empty val before bootstrap fill).
        if not any(p.is_file() and p.suffix.lower() == ".txt" for p in labels_dir.iterdir()):
            continue
        warnings.extend(deps.assert_label_class_ids_compatible(labels_dir, class_map))
    return warnings
