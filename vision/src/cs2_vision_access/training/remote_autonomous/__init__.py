"""Documented compatibility facade for autonomous remote training.

Only the established public entry points and the patchable ``deps`` module are
retained.  Private canonical submodules intentionally have no legacy path.
"""

from cs2_vision_access.workflows.training.remote_autonomous import (
    AutonomousReport,
    IterationReport,
    _rank_and_write_uncertain_review,
    bootstrap_labels_with_yolo_person,
    bootstrap_with_edgesam,
    ensure_edgesam_assets,
    materialize_held_out_split,
    promote_cs2_10k_holdout_to_session_split,
    resolve_remote_dataset_zip,
    run_autonomous_loop,
)

from . import deps

__all__ = [
    "AutonomousReport",
    "IterationReport",
    "deps",
    "bootstrap_labels_with_yolo_person",
    "bootstrap_with_edgesam",
    "ensure_edgesam_assets",
    "materialize_held_out_split",
    "promote_cs2_10k_holdout_to_session_split",
    "resolve_remote_dataset_zip",
    "run_autonomous_loop",
    "_rank_and_write_uncertain_review",
]
