"""Shared path and I/O helpers for evaluation modules."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

from cs2_vision_access.workflows.evaluation.errors import EvaluationError


def _require_path_segment(value: str, *, label: str) -> str:
    """Reject empty names, separators, and ``..`` path components."""
    if not isinstance(value, str) or not value or value.strip() != value:
        raise EvaluationError(f"{label} must be a non-empty path segment")
    if value in {".", ".."}:
        raise EvaluationError(f"{label} must not be '.' or '..'")
    if "/" in value or "\\" in value:
        raise EvaluationError(f"{label} must not contain path separators")
    if Path(value).name != value:
        raise EvaluationError(f"{label} must be a single path segment")
    return value


def _write_text_atomic(path: Path, text: str) -> None:
    """Write via a temp file in the same directory, then ``os.replace``."""
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
