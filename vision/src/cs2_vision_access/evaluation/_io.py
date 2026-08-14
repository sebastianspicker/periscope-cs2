"""Shared path and I/O helpers for evaluation modules."""

from __future__ import annotations

from pathlib import Path

from cs2_vision_access.evaluation.errors import EvaluationError


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
