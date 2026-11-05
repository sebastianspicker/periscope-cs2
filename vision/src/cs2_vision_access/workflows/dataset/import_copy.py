"""Copy/hardlink and session.json write helpers for box-dataset import."""

from __future__ import annotations

import json
import os
import shutil
import tempfile
from pathlib import Path

from cs2_vision_access.workflows.dataset.frames import SessionProvenance
from cs2_vision_access.workflows.dataset.import_layout import ImportBoxDatasetError


def prepare_session_directory(session_dir: Path, *, overwrite: bool) -> Path | None:
    """Ready ``session_dir``; with ``overwrite`` return the backup dir holding the old content.

    The old session is moved aside, never deleted: the caller drops it with
    :func:`discard_backup` after the import succeeds or restores it with
    :func:`restore_backup` on failure.
    """
    if session_dir.is_symlink():
        raise ImportBoxDatasetError(f"session directory must not be a symlink: {session_dir}")
    if not session_dir.exists():
        session_dir.mkdir(parents=True, exist_ok=True)
        return None
    if not session_dir.is_dir():
        raise ImportBoxDatasetError(f"session path exists and is not a directory: {session_dir}")
    occupied = any(session_dir.iterdir())
    if not occupied:
        return None
    if not overwrite:
        raise ImportBoxDatasetError(
            f"session directory is not empty: {session_dir}; pass --overwrite to replace"
        )
    backup = Path(tempfile.mkdtemp(dir=session_dir.parent, prefix=f".{session_dir.name}.old-"))
    os.replace(session_dir, backup / session_dir.name)
    session_dir.mkdir()
    return backup


def _require_own_sibling(path: Path, session_dir: Path) -> None:
    # Allowlist: only ``session_dir`` itself or a backup dir created beside it.
    if path.is_symlink() or path.resolve().parent != session_dir.resolve().parent:
        raise ImportBoxDatasetError(f"refusing to remove unexpected path: {path}")
    if path != session_dir and not path.name.startswith(f".{session_dir.name}.old-"):
        raise ImportBoxDatasetError(f"refusing to remove unexpected path: {path}")


def discard_backup(backup: Path, session_dir: Path) -> None:
    _require_own_sibling(backup, session_dir)
    shutil.rmtree(backup)


def restore_backup(backup: Path, session_dir: Path) -> None:
    """Drop the partial new session and put the previous one back."""
    _require_own_sibling(backup, session_dir)
    _require_own_sibling(session_dir, session_dir)
    shutil.rmtree(session_dir)
    os.replace(backup / session_dir.name, session_dir)
    backup.rmdir()


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
