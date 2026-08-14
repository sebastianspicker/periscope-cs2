"""Path safety helpers for cue log files."""

from __future__ import annotations

from pathlib import Path

from cs2_vision_access.cues.errors import CueLogError


def _prepare_cue_log_path(path: str | Path, *, overwrite: bool) -> Path:
    destination = Path(path)
    if destination.suffix.lower() not in {".jsonl", ".json"}:
        raise CueLogError("cue log destination must use the .jsonl or .json extension")
    if destination.is_symlink():
        raise CueLogError("cue log destination must not be a symlink")
    if destination.exists() and not destination.is_file():
        raise CueLogError("cue log destination exists and is not a regular file")
    if destination.exists() and not overwrite:
        raise CueLogError(f"cue log already exists: {destination}; pass overwrite=True to replace")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise CueLogError("cue log parent must not be a symlink")
    return destination.resolve()


def _regular_cue_file(path: str | Path) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise CueLogError("cue log path must not be a symlink")
    if candidate.parent.is_symlink():
        raise CueLogError("cue log parent must not be a symlink")
    if not candidate.is_file():
        raise CueLogError(f"cue log is not a regular file: {candidate}")
    if candidate.suffix.lower() not in {".jsonl", ".json"}:
        raise CueLogError("cue log must use the .jsonl or .json format")
    return candidate.resolve()
