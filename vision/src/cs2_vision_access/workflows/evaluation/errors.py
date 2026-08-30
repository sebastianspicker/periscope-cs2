"""Evaluation error types."""

from __future__ import annotations


class EvaluationError(ValueError):
    """A metric input or evaluation payload failed a deterministic check."""
