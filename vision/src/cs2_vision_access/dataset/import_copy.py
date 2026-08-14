"""Copy/hardlink and session.json write helpers for box-dataset import."""

from __future__ import annotations

import json
import os
import shutil
import tempfile
from pathlib import Path

from cs2_vision_access.dataset.import_layout import ImportBoxDatasetError
from cs2_vision_access.frames import SessionProvenance


def prepare_session_directory(session_dir: Path, *, overwrite: bool) -> None:
    if session_dir.is_symlink():
        raise ImportBoxDatasetError(f"session directory must not be a symlink: {session_dir}")
    if not session_dir.exists():
        session_dir.mkdir(parents=True, exist_ok=True)
        return
    if not session_dir.is_dir():
        raise ImportBoxDatasetError(f"session path exists and is not a directory: {session_dir}")
    occupied = any(session_dir.iterdir())
    if not occupied:
        return
    if not overwrite:
        raise ImportBoxDatasetError(
            f"session directory is not empty: {session_dir}; pass --overwrite to replace"
        )
    for child in session_dir.iterdir():
        if child.is_symlink() or child.is_file():
            child.unlink()
        elif child.is_dir():
            shutil.rmtree(child)


def copy_or_link(source: Path, destination: Path, *, link: bool) -> bool:
    if source.is_symlink():
        raise ImportBoxDatasetError(f"source path must not be a symlink: {source}")
    if destination.exists() or destination.is_symlink():
        raise ImportBoxDatasetError(f"destination already exists: {destination}")
    if link:
        try:
            os.link(source, destination)
            return True
        except OSError:
            # Cross-device or unsupported; fall back to copy.
            shutil.copy2(source, destination)
            return False
    shutil.copy2(source, destination)
    return False


def write_session_json(destination: Path, provenance: SessionProvenance) -> Path:
    if destination.is_symlink():
        raise ImportBoxDatasetError("session.json destination must not be a symlink")
    if destination.exists():
        raise ImportBoxDatasetError(f"session.json already exists: {destination}")
    payload = json.dumps(provenance.as_json(), indent=2, sort_keys=True) + "\n"
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
        raise ImportBoxDatasetError(f"could not write session.json: {error}") from error
    return destination.resolve()
