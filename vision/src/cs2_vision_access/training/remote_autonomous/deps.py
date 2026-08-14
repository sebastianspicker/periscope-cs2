"""Patchable external bindings used by remote_autonomous phases.

Tests should patch symbols on this module, e.g.::

    patch("cs2_vision_access.training.remote_autonomous.deps.train", ...)

Phase code must call via ``deps.train(...)`` (module attribute lookup) so
patches take effect. Package ``__init__`` re-exports the same names for the
public / historical import path.
"""

from __future__ import annotations

from cs2_vision_access.training.active_learning import rank_uncertain_queue_soft
from cs2_vision_access.training.bootstrap_labels import (
    bootstrap_class_id,
    bootstrap_labels_with_yolo_person,
)
from cs2_vision_access.training.cloud import (
    OUTPUT_MANIFEST_NAME,
    OUTPUT_ONNX_NAME,
    create_manifest,
    extract_dataset,
    find_dataset_zip,
    install_dependencies,
    package_outputs,
    train,
)
from cs2_vision_access.training.contracts import (
    Layout,
    assert_label_class_ids_compatible,
    normalize_class_names,
    resolve_train_hyperparameters,
    write_dataset_yaml,
)
from cs2_vision_access.training.dataset_zip import (
    count_images,
    count_labels,
    find_image_for_stem,
)
from cs2_vision_access.training.prepare_lib import (
    bootstrap_with_edgesam,
    ensure_edgesam_assets,
)
from cs2_vision_access.training.progress_report import (
    find_ultralytics_run_dir,
    parse_ultralytics_results_csv,
    write_progress_report,
)
from cs2_vision_access.training.self_train import run_self_train_iteration
from cs2_vision_access.training.self_train_schedule import resolve_conf_bands
from cs2_vision_access.training.teacher_strategy import BestPackageTeacherStrategy
from cs2_vision_access.training.train_core import auto_train_device, is_oom_error

try:
    from ultralytics import YOLO
except ImportError:  # pragma: no cover — optional until deps install
    YOLO = None  # type: ignore[assignment,misc]

__all__ = [
    "OUTPUT_MANIFEST_NAME",
    "OUTPUT_ONNX_NAME",
    "BestPackageTeacherStrategy",
    "Layout",
    "YOLO",
    "assert_label_class_ids_compatible",
    "auto_train_device",
    "bootstrap_class_id",
    "bootstrap_labels_with_yolo_person",
    "bootstrap_with_edgesam",
    "count_images",
    "count_labels",
    "create_manifest",
    "ensure_edgesam_assets",
    "extract_dataset",
    "find_dataset_zip",
    "find_image_for_stem",
    "find_ultralytics_run_dir",
    "install_dependencies",
    "is_oom_error",
    "normalize_class_names",
    "package_outputs",
    "parse_ultralytics_results_csv",
    "rank_uncertain_queue_soft",
    "resolve_conf_bands",
    "resolve_train_hyperparameters",
    "run_self_train_iteration",
    "train",
    "write_dataset_yaml",
    "write_progress_report",
]
