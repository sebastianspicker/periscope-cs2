"""Shared exception types for auto-train stages."""

from __future__ import annotations


class AutoTrainStageError(RuntimeError):
    """A pipeline stage failed closed."""


class HumanGateBlocked(Exception):
    """Operator gate requested a stop before train (exit code 2)."""

    def __init__(self, message: str) -> None:
        super().__init__(message)
        self.message = message
