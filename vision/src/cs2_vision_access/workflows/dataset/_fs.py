"""Filesystem helpers shared by dataset validation and related tooling."""

from __future__ import annotations

import os
from pathlib import Path


def files_under(directory: Path, extensions: frozenset[str]) -> tuple[list[Path], list[Path]]:
    """Walk ``directory`` without following links; return matching files and symlinks."""
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


def relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()
