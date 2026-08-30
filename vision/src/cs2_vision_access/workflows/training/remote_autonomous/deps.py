"""Compatibility alias for patchable remote-autonomous bindings.

The implementation lives beside the package so phase modules can depend on a
neutral leaf module instead of importing this package's re-exporting
``__init__``. Replacing this module in ``sys.modules`` preserves the documented
patch target: ``remote_autonomous.deps.train`` is the same mutable object read
by every phase.
"""

from __future__ import annotations

import sys

from cs2_vision_access.workflows.training import remote_autonomous_bindings as _bindings
from cs2_vision_access.workflows.training.remote_autonomous_bindings import (
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

__all__ = [
    "YOLO",
    "bootstrap_labels_with_yolo_person",
    "bootstrap_with_edgesam",
    "create_manifest",
    "ensure_edgesam_assets",
    "install_dependencies",
    "package_outputs",
    "run_self_train_iteration",
    "train",
]

sys.modules[__name__] = _bindings
