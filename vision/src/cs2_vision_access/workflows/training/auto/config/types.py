"""Auto-train config types, constants, and dataclasses."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.contracts import PRODUCT_CLASSES


class AutoTrainConfigError(ValueError):
    """Configuration is missing, invalid, or inconsistent."""


SUPPORTED_MODES = frozenset({"session_split", "flat_cloud"})
SUPPORTED_BACKENDS = frozenset({"local", "cloud"})
SUPPORTED_LABEL_TEACHERS = frozenset({"coco_person", "edgesam", "none"})
SCHEMA_VERSION = 1


@dataclass(frozen=True)
class PathsConfig:
    work_root: Path = Path("artifacts/auto")


@dataclass(frozen=True)
class SourcesConfig:
    dataset_zip: Path | None = None
    prebuilt_dataset_root: Path | None = None
    prebuilt_flat_root: Path | None = None
    staging_root: Path | None = None
    split_plan: Path | None = None
    videos: tuple[Path, ...] = ()
    every_n_frames: int = 30
    max_saved_frames: int = 1000


@dataclass(frozen=True)
class TrainConfig:
    backend: str = "cloud"
    profile: str = "cloud_t4"
    allow_leaky_val: bool = False
    epochs: int | None = None
    batch: int | None = None
    image_size: int | None = None
    device: str = "cpu"
    # None → profile base_model wins in resolve_train_hyperparameters.
    base_model: str | None = None
    base_model_origin: str = "ultralytics-official"
    exported_model_license: str = "AGPL-3.0-only"
    allow_model_download: bool = True
    # Product single-class default; set class_names explicitly for multi-class
    # (e.g. Vombit ct/t). Prefer PRODUCT_CLASSES for product mode.
    class_names: dict[int, str] = field(default_factory=lambda: dict(PRODUCT_CLASSES))
    smoke: bool = False
    min_labels: int = 1
    min_map50: float | None = None
    resume_ultralytics: bool = True


@dataclass(frozen=True)
class ExportConfig:
    promote_to_run_models: bool = True
    package_zip_name: str = "model-package.zip"
    smoke_enabled: bool = True
    smoke_required: bool = False


@dataclass(frozen=True)
class LabelConfig:
    """Optional label bootstrap before validate.

    ``teacher`` selects how sparse labels are filled:
    - ``coco_person`` (default): YOLO-seg COCO person bootstrap
    - ``edgesam``: Vombit detector + EdgeSAM masks via prepare_lib
    - ``none``: skip bootstrap even when sparse

    Autonomous runs enable bootstrap but **do not** force ``teacher=edgesam``
    (EdgeSAM needs ONNX models under ``artifacts_dir`` or explicit paths).
    Set ``download_edgesam=True`` to allow teacher ONNX download when missing.
    """

    enabled: bool = True
    bootstrap: bool = True
    conf: float = 0.25
    # session_split: which image/label subdirs to bootstrap (default train only).
    bootstrap_splits: tuple[str, ...] = ("train",)
    # Bootstrap when labeled/images ratio is below this (and bootstrap enabled).
    min_label_ratio: float = 0.05
    # When True, hard-fail if labels remain sparse after bootstrap attempts.
    required: bool = False
    # Label teacher for sparse datasets.
    teacher: str = "coco_person"  # "coco_person" | "edgesam" | "none"
    # EdgeSAM / Vombit paths (optional if discoverable under artifacts_dir)
    detector: Path | None = None
    detector_manifest: Path | None = None
    encoder: Path | None = None
    decoder: Path | None = None
    artifacts_dir: Path = Path("artifacts")
    edgesam_confidence: float = 0.4
    sample_rate: float = 3.0  # video fps sampling
    max_frames: int = 0
    collapse_to_player: bool = True
    keep_negatives: bool = True
    # if edgesam fails soft and bootstrap True, fall back to coco_person
    coco_fallback: bool = True
    # Offline default: discover only. When True, ensure_edgesam_assets may download.
    download_edgesam: bool = False


@dataclass(frozen=True)
class EvalConfig:
    """Optional post-export mask evaluation on a dataset split."""

    enabled: bool = True
    required: bool = False
    split: str = "val"
    conf: float = 0.25


@dataclass(frozen=True)
class SelfTrainConfig:
    """Optional post-export self-train / pseudo-label stage.

    Off by default. When enabled, runs student ONNX pseudo-labeling on the
    train (or flat) split only, then continues to eval.

    Multi-iter: ``iterations`` self_train→retrain cycles after the initial
    train/export. When ``iterations > 1``, retrain is forced on after accepted
    labels. ``conf_schedule`` raises conf each iteration like remote autonomous.
    ``autonomous`` is a parse-time convenience that enables multi-iter defaults.

    Teacher gate: set explicit ``teacher_model`` + ``teacher_manifest`` for
    iteration 1. With ``use_teacher`` and multi-iter, ``teacher_strategy``
    (default ``prev_student``) selects the teacher for each iteration.
    """

    enabled: bool = False
    required: bool = False
    conf_threshold: float = 0.5
    write_policy: str = "overwrite_pseudo"
    max_frames: int = 0
    conf_low: float | None = None  # default 0.7 * conf if None and enabled
    conf_high: float | None = None
    write_uncertain_queue: bool = True
    # optional second train+export after self-label (off by default)
    retrain: bool = False
    retrain_required: bool = False  # hard fail on retrain error if true
    # Teacher gate: only applied when paths set (iter1) or multi-iter prev onnx
    use_teacher: bool = True
    teacher_min_iou: float = 0.3
    teacher_model: Path | None = None
    teacher_manifest: Path | None = None
    # Named teacher resolution strategy (see teacher_strategy.py)
    teacher_strategy: str = "prev_student"
    # Multi-iter closed loop after initial train/export
    iterations: int = 1
    # total self_train→retrain cycles after initial train/export
    # 1 + retrain=true = one cycle (current-ish)
    # 3 = three self_train+retrain cycles
    conf_schedule: bool = True
    # conf grows each iteration: min(0.85, base * (1 + 0.05*(it-1)))
    stop_on_no_growth: bool = True
    # stop early if accepted labels == 0 for a cycle (see max_plateau_iters)
    max_plateau_iters: int = 2
    # stop if consecutive cycles with accepted==0 reach this
    autonomous: bool = False
    # parse convenience: enabled+retrain+conf_schedule+use_teacher


@dataclass(frozen=True)
class ResumeConfig:
    enabled: bool = True


@dataclass(frozen=True)
class HumanGateConfig:
    """Operator gate after validate; ``block=True`` forces exit code 2 before train.

    When blocking, the pipeline writes ``progress/human_gate.json`` and
    enriches the exit message with any existing ``uncertain_review.json`` path.
    """

    enabled: bool = False
    block: bool = False
    message: str = "human_gate blocked: operator review required before train"


@dataclass(frozen=True)
class AutoTrainConfig:
    schema_version: int
    mode: str
    run_id: str
    paths: PathsConfig = field(default_factory=PathsConfig)
    sources: SourcesConfig = field(default_factory=SourcesConfig)
    train: TrainConfig = field(default_factory=TrainConfig)
    export: ExportConfig = field(default_factory=ExportConfig)
    label: LabelConfig = field(default_factory=LabelConfig)
    eval: EvalConfig = field(default_factory=EvalConfig)
    self_train: SelfTrainConfig = field(default_factory=SelfTrainConfig)
    resume: ResumeConfig = field(default_factory=ResumeConfig)
    human_gate: HumanGateConfig = field(default_factory=HumanGateConfig)

    def to_dict(self) -> dict[str, Any]:
        """Serialise to a JSON-friendly mapping (paths as strings)."""
        return {
            "schema_version": self.schema_version,
            "mode": self.mode,
            "run_id": self.run_id,
            "paths": {"work_root": str(self.paths.work_root)},
            "sources": {
                "dataset_zip": _path_str(self.sources.dataset_zip),
                "prebuilt_dataset_root": _path_str(self.sources.prebuilt_dataset_root),
                "prebuilt_flat_root": _path_str(self.sources.prebuilt_flat_root),
                "staging_root": _path_str(self.sources.staging_root),
                "split_plan": _path_str(self.sources.split_plan),
                "videos": [str(p) for p in self.sources.videos],
                "every_n_frames": self.sources.every_n_frames,
                "max_saved_frames": self.sources.max_saved_frames,
            },
            "train": {
                "backend": self.train.backend,
                "profile": self.train.profile,
                "allow_leaky_val": self.train.allow_leaky_val,
                "epochs": self.train.epochs,
                "batch": self.train.batch,
                "image_size": self.train.image_size,
                "device": self.train.device,
                "base_model": self.train.base_model,
                "base_model_origin": self.train.base_model_origin,
                "exported_model_license": self.train.exported_model_license,
                "allow_model_download": self.train.allow_model_download,
                "class_names": {str(k): v for k, v in sorted(self.train.class_names.items())},
                "smoke": self.train.smoke,
                "min_labels": self.train.min_labels,
                "min_map50": self.train.min_map50,
                "resume_ultralytics": self.train.resume_ultralytics,
            },
            "export": {
                "promote_to_run_models": self.export.promote_to_run_models,
                "package_zip_name": self.export.package_zip_name,
                "smoke_enabled": self.export.smoke_enabled,
                "smoke_required": self.export.smoke_required,
            },
            "label": {
                "enabled": self.label.enabled,
                "bootstrap": self.label.bootstrap,
                "conf": self.label.conf,
                "bootstrap_splits": list(self.label.bootstrap_splits),
                "min_label_ratio": self.label.min_label_ratio,
                "required": self.label.required,
                "teacher": self.label.teacher,
                "detector": _path_str(self.label.detector),
                "detector_manifest": _path_str(self.label.detector_manifest),
                "encoder": _path_str(self.label.encoder),
                "decoder": _path_str(self.label.decoder),
                "artifacts_dir": str(self.label.artifacts_dir),
                "edgesam_confidence": self.label.edgesam_confidence,
                "sample_rate": self.label.sample_rate,
                "max_frames": self.label.max_frames,
                "collapse_to_player": self.label.collapse_to_player,
                "keep_negatives": self.label.keep_negatives,
                "coco_fallback": self.label.coco_fallback,
                "download_edgesam": self.label.download_edgesam,
            },
            "eval": {
                "enabled": self.eval.enabled,
                "required": self.eval.required,
                "split": self.eval.split,
                "conf": self.eval.conf,
            },
            "self_train": {
                "enabled": self.self_train.enabled,
                "required": self.self_train.required,
                "conf_threshold": self.self_train.conf_threshold,
                "write_policy": self.self_train.write_policy,
                "max_frames": self.self_train.max_frames,
                "conf_low": self.self_train.conf_low,
                "conf_high": self.self_train.conf_high,
                "write_uncertain_queue": self.self_train.write_uncertain_queue,
                "retrain": self.self_train.retrain,
                "retrain_required": self.self_train.retrain_required,
                "use_teacher": self.self_train.use_teacher,
                "teacher_min_iou": self.self_train.teacher_min_iou,
                "teacher_model": _path_str(self.self_train.teacher_model),
                "teacher_manifest": _path_str(self.self_train.teacher_manifest),
                "teacher_strategy": self.self_train.teacher_strategy,
                "iterations": self.self_train.iterations,
                "conf_schedule": self.self_train.conf_schedule,
                "stop_on_no_growth": self.self_train.stop_on_no_growth,
                "max_plateau_iters": self.self_train.max_plateau_iters,
                "autonomous": self.self_train.autonomous,
            },
            "resume": {"enabled": self.resume.enabled},
            "human_gate": {
                "enabled": self.human_gate.enabled,
                "block": self.human_gate.block,
                "message": self.human_gate.message,
            },
        }


def _path_str(value: Path | None) -> str | None:
    return str(value) if value is not None else None
