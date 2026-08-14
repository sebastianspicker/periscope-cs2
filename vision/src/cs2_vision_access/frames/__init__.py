"""Deterministic frame extraction for local annotation — data models and extractor.

Split from the original ``frames.py`` monolith: data models live in
``frames.models`` and extraction logic in ``frames.extract``.
"""

from __future__ import annotations

from cs2_vision_access.frames.extract import extract_frames
from cs2_vision_access.frames.models import (
    RIGHTS_STATUS_PLACEHOLDER,
    SCHEMA_VERSION,
    SESSION_FILENAME,
    FrameExtractionError,
    FrameExtractionSummary,
    FramePolicy,
    RightsPlaceholder,
    SessionProvenance,
    SessionProvenanceError,
)

__all__ = [
    "SESSION_FILENAME",
    "SCHEMA_VERSION",
    "RIGHTS_STATUS_PLACEHOLDER",
    "FrameExtractionError",
    "FrameExtractionSummary",
    "FramePolicy",
    "RightsPlaceholder",
    "SessionProvenance",
    "SessionProvenanceError",
    "extract_frames",
]
