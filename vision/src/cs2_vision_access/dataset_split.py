"""Assemble YOLO train/val/test trees from staged session directories.

Operators annotate per-session staging folders, then assign whole ``session_id``
values to train, val, or test. Adjacent frames within a session are never
randomly split across partitions — that would measure memorization, not
generalization.

Implementation lives in :mod:`cs2_vision_access.workflows.dataset.split`; this module
re-exports the public API for stable import paths.
"""

from __future__ import annotations

from cs2_vision_access.workflows.dataset.split import (
    AssembleDatasetSummary,
    DatasetSplitError,
    SessionSplitPlan,
    assemble_dataset,
    build_split_plan,
    discover_session_directories,
    load_split_plan,
)

__all__ = [
    "AssembleDatasetSummary",
    "DatasetSplitError",
    "SessionSplitPlan",
    "assemble_dataset",
    "build_split_plan",
    "discover_session_directories",
    "load_split_plan",
]
