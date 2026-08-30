"""Filesystem helpers for box-to-mask bootstrap."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

from cs2_vision_access.workflows.dataset import IMAGE_EXTENSIONS
from cs2_vision_access.workflows.labeling.types import BootstrapError


def require_existing_dir(path: str | Path, label: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise BootstrapError(f"{label} must not be a symlink")
    if not candidate.is_dir():
        raise BootstrapError(f"{label} is not a directory: {candidate}")
    return candidate


def collect_images(root: Path) -> list[Path]:
    files, symlinks = files_under(root, IMAGE_EXTENSIONS)
    if symlinks:
        raise BootstrapError(
            "symlinks are not allowed under images directory: " + symlinks[0].as_posix()
        )
    return files


def collect_labels(root: Path) -> list[Path]:
    files, symlinks = files_under(root, frozenset({".txt"}))
    if symlinks:
        raise BootstrapError(
            "symlinks are not allowed under labels directory: " + symlinks[0].as_posix()
        )
    return files


def files_under(directory: Path, extensions: frozenset[str]) -> tuple[list[Path], list[Path]]:
    files: list[Path] = []
    symlinks: list[Path] = []
    for current_root, directory_names, filenames in os.walk(directory, followlinks=False):
        current = Path(current_root)
        for name in sorted(directory_names):
            candidate = current / name
            if candidate.is_symlink():
                symlinks.append(candidate)
        directory_names[:] = [name for name in directory_names if not (current / name).is_symlink()]
        for name in sorted(filenames):
            candidate = current / name
            if candidate.is_symlink():
                symlinks.append(candidate)
            elif candidate.suffix.lower() in extensions:
                files.append(candidate)
    return sorted(files), sorted(symlinks)


def write_text_atomic(destination: Path, payload: str) -> None:
    if destination.is_symlink():
        raise BootstrapError(f"destination must not be a symlink: {destination}")
    if destination.parent.is_symlink():
        raise BootstrapError(f"destination parent must not be a symlink: {destination.parent}")
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(payload)
            handle.flush()
        Path(temporary_name).replace(destination)
    except OSError as error:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
        raise BootstrapError(f"could not write {destination}: {error}") from error
