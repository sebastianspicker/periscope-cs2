"""Path and JSON helpers shared across study package modules."""

from __future__ import annotations

import json
import os
import tempfile
from collections.abc import Mapping, Sequence
from pathlib import Path

from cs2_vision_access.workflows.study.errors import StudyError


def _write_json_atomic(path: Path, payload: Mapping[str, object]) -> Path:
    if path.is_symlink():
        raise StudyError(f"json destination must not be a symlink: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.parent.is_symlink():
        raise StudyError("json destination parent must not be a symlink")
    text = json.dumps(dict(payload), indent=2, sort_keys=True) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, path)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return path.resolve()


def _regular_json_file(path: str | Path, *, label: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise StudyError(f"{label} path must not be a symlink")
    if candidate.parent.is_symlink():
        raise StudyError(f"{label} parent must not be a symlink")
    if not candidate.is_file():
        raise StudyError(f"{label} is not a regular file: {candidate}")
    if candidate.suffix.lower() != ".json":
        raise StudyError(f"{label} must use the .json format")
    return candidate.resolve()


def _resolve_path(value: str, *, base_directory: Path) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = base_directory / candidate
    return candidate


def _resolve_existing_path(value: str, *, base_directory: Path, label: str) -> Path:
    candidate = _resolve_path(value, base_directory=base_directory)
    if candidate.is_symlink():
        raise StudyError(f"{label} path must not be a symlink")
    if not candidate.is_file():
        raise StudyError(f"{label} is not a regular file: {candidate}")
    return candidate.resolve()


def _path_segment(value: object, *, label: str) -> str:
    if not isinstance(value, str) or not value or value.strip() != value:
        raise StudyError(f"{label} must be a non-empty path segment")
    if value in {".", ".."}:
        raise StudyError(f"{label} must not be '.' or '..'")
    if "/" in value or "\\" in value or "\x00" in value:
        raise StudyError(f"{label} must not contain path separators or NUL")
    if Path(value).name != value:
        raise StudyError(f"{label} must be a single path segment")
    return value


def _required_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise StudyError(f"{field} must be a non-empty string")
    if "\x00" in value:
        raise StudyError(f"{field} must not contain NUL")
    return value.strip()


def _optional_text(value: object, field: str) -> str | None:
    if value is None:
        return None
    return _required_text(value, field)


def _optional_color(value: object, field: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise StudyError(f"{field} must be a #RRGGBB string or null")
    return value


def _optional_int(value: object, field: str) -> int | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int):
        raise StudyError(f"{field} must be an integer or null")
    return value


def _optional_float(value: object, field: str) -> float | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise StudyError(f"{field} must be a number or null")
    return float(value)


def _optional_bool(value: object, field: str) -> bool | None:
    if value is None:
        return None
    if not isinstance(value, bool):
        raise StudyError(f"{field} must be a boolean or null")
    return value


def _mean(values: Sequence[float]) -> float:
    if not values:
        return 0.0
    return float(sum(values) / len(values))
