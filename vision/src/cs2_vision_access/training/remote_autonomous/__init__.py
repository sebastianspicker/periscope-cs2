"""Notebooks call :func:`run_autonomous_loop` for fully autonomous iterative
silhouette training toward CS2 player shapes (product class ``player``).

Colab/Kaggle entry point: bootstrap sparse labels (default YOLO-seg COCO person;
opt-in Vombit+EdgeSAM via ``use_edgesam=True``), hold out val, then train →
shape self-train for *N* iterations (conf schedule + multi-iter teacher gate),
and package the best ONNX + manifest. Targets free-tier notebooks that run
top-to-bottom without manual ``cloud.py`` uploads when the package is installed
(or vendored).

Example::

    from cs2_vision_access.training.remote_autonomous import run_autonomous_loop
    report = run_autonomous_loop(dataset_zip="data.zip", iterations=3)

Patchable symbols used by phases live on :mod:`.deps`. Prefer::

    patch("cs2_vision_access.training.remote_autonomous.deps.train", ...)
"""

from __future__ import annotations

from . import deps
from .deps import (  # noqa: F401 — public / historical re-exports for patches
    YOLO,
    bootstrap_labels_with_yolo_person,
    bootstrap_with_edgesam,
    create_manifest,
    ensure_edgesam_assets,
    install_dependencies,
    package_outputs,
    run_self_train_iteration,
    train,
)
from .loop import run_autonomous_loop
from .models import AutonomousReport, IterationReport
from .resolve import resolve_remote_dataset_zip
from .splits import materialize_held_out_split, promote_cs2_10k_holdout_to_session_split
from .state import _rank_and_write_uncertain_review

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
