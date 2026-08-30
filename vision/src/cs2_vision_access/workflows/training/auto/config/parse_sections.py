"""Section parsers for auto-train config."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.contracts import resolve_profile

from .parse_utils import (
    _optional_float,
    _optional_int,
    _optional_path,
    _parse_class_names,
    _parse_conf,
)
from .types import (
    SUPPORTED_BACKENDS,
    SUPPORTED_LABEL_TEACHERS,
    AutoTrainConfigError,
    EvalConfig,
    ExportConfig,
    HumanGateConfig,
    LabelConfig,
    PathsConfig,
    ResumeConfig,
    SelfTrainConfig,
    SourcesConfig,
    TrainConfig,
)


def _parse_paths(raw: Mapping[str, Any]) -> PathsConfig:
    work = raw.get("work_root", "artifacts/auto")
    if not isinstance(work, str) or not work.strip():
        raise AutoTrainConfigError("paths.work_root must be a non-empty string")
    return PathsConfig(work_root=Path(work))


def _parse_sources(raw: Mapping[str, Any]) -> SourcesConfig:
    videos_raw = raw.get("videos") or []
    if not isinstance(videos_raw, list):
        raise AutoTrainConfigError("sources.videos must be a list of paths")
    videos: list[Path] = []
    for item in videos_raw:
        if not isinstance(item, str):
            raise AutoTrainConfigError("sources.videos entries must be strings")
        videos.append(Path(item))
    every_n = raw.get("every_n_frames", 30)
    max_frames = raw.get("max_saved_frames", 1000)
    if isinstance(every_n, bool) or not isinstance(every_n, int) or every_n <= 0:
        raise AutoTrainConfigError("sources.every_n_frames must be a positive int")
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames <= 0:
        raise AutoTrainConfigError("sources.max_saved_frames must be a positive int")
    return SourcesConfig(
        dataset_zip=_optional_path(raw.get("dataset_zip"), "sources.dataset_zip"),
        prebuilt_dataset_root=_optional_path(
            raw.get("prebuilt_dataset_root"), "sources.prebuilt_dataset_root"
        ),
        prebuilt_flat_root=_optional_path(
            raw.get("prebuilt_flat_root"), "sources.prebuilt_flat_root"
        ),
        staging_root=_optional_path(raw.get("staging_root"), "sources.staging_root"),
        split_plan=_optional_path(raw.get("split_plan"), "sources.split_plan"),
        videos=tuple(videos),
        every_n_frames=every_n,
        max_saved_frames=max_frames,
    )


def _parse_train(raw: Mapping[str, Any]) -> TrainConfig:
    backend = str(raw.get("backend", "cloud")).strip()
    if backend not in SUPPORTED_BACKENDS:
        raise AutoTrainConfigError(f"train.backend must be one of {sorted(SUPPORTED_BACKENDS)}")
    profile = str(raw.get("profile", "cloud_t4")).strip()
    try:
        resolve_profile(profile)
    except ValueError as error:
        raise AutoTrainConfigError(str(error)) from error
    min_labels = raw.get("min_labels", 1)
    if isinstance(min_labels, bool) or not isinstance(min_labels, int) or min_labels < 0:
        raise AutoTrainConfigError("train.min_labels must be a non-negative int")
    device = raw.get("device", "cpu")
    if not isinstance(device, str) or not device.strip():
        raise AutoTrainConfigError("train.device must be a non-empty string")
    min_map50 = _optional_float(raw.get("min_map50"), "train.min_map50")
    if min_map50 is not None and min_map50 < 0.0:
        raise AutoTrainConfigError("train.min_map50 must be non-negative")
    # Only set base_model when the key is present so profiles can supply it.
    base_model: str | None = None
    if "base_model" in raw and raw["base_model"] is not None:
        if not isinstance(raw["base_model"], str) or not str(raw["base_model"]).strip():
            raise AutoTrainConfigError("train.base_model must be a non-empty string or null")
        base_model = str(raw["base_model"]).strip()
    return TrainConfig(
        backend=backend,
        profile=profile,
        allow_leaky_val=bool(raw.get("allow_leaky_val", False)),
        epochs=_optional_int(raw.get("epochs"), "train.epochs"),
        batch=_optional_int(raw.get("batch"), "train.batch"),
        image_size=_optional_int(raw.get("image_size"), "train.image_size"),
        device=device.strip(),
        base_model=base_model,
        base_model_origin=str(raw.get("base_model_origin", "ultralytics-official")),
        exported_model_license=str(raw.get("exported_model_license", "AGPL-3.0-only")),
        allow_model_download=bool(raw.get("allow_model_download", True)),
        class_names=_parse_class_names(raw.get("class_names")),
        smoke=bool(raw.get("smoke", False)),
        min_labels=min_labels,
        min_map50=min_map50,
        resume_ultralytics=bool(raw.get("resume_ultralytics", True)),
    )


def _parse_export(raw: Mapping[str, Any]) -> ExportConfig:
    name = raw.get("package_zip_name", "model-package.zip")
    if not isinstance(name, str) or not name.strip():
        raise AutoTrainConfigError("export.package_zip_name must be a non-empty string")
    return ExportConfig(
        promote_to_run_models=bool(raw.get("promote_to_run_models", True)),
        package_zip_name=name.strip(),
        smoke_enabled=bool(raw.get("smoke_enabled", True)),
        smoke_required=bool(raw.get("smoke_required", False)),
    )


def _parse_bootstrap_splits(raw: object) -> tuple[str, ...]:
    if raw is None:
        return ("train",)
    if isinstance(raw, str):
        split = raw.strip()
        if not split:
            raise AutoTrainConfigError("label.bootstrap_splits entries must be non-empty")
        return (split,)
    if isinstance(raw, (list, tuple)):
        splits: list[str] = []
        for item in raw:
            if not isinstance(item, str) or not item.strip():
                raise AutoTrainConfigError(
                    "label.bootstrap_splits must be a list of non-empty strings"
                )
            splits.append(item.strip())
        return tuple(splits)
    raise AutoTrainConfigError("label.bootstrap_splits must be a string or list of strings")


def _parse_label(raw: Mapping[str, Any]) -> LabelConfig:
    min_label_ratio = raw.get("min_label_ratio", 0.05)
    if isinstance(min_label_ratio, bool) or not isinstance(min_label_ratio, (int, float)):
        raise AutoTrainConfigError("label.min_label_ratio must be a number in [0, 1]")
    ratio = float(min_label_ratio)
    if not 0.0 <= ratio <= 1.0:
        raise AutoTrainConfigError("label.min_label_ratio must be in [0, 1]")

    teacher_raw = raw.get("teacher", "coco_person")
    if not isinstance(teacher_raw, str) or not teacher_raw.strip():
        raise AutoTrainConfigError(
            f"label.teacher must be a non-empty string (one of {sorted(SUPPORTED_LABEL_TEACHERS)})"
        )
    teacher = teacher_raw.strip().lower()
    if teacher not in SUPPORTED_LABEL_TEACHERS:
        raise AutoTrainConfigError(
            f"label.teacher must be one of {sorted(SUPPORTED_LABEL_TEACHERS)}"
        )

    artifacts_raw = raw.get("artifacts_dir", "artifacts")
    if not isinstance(artifacts_raw, str) or not artifacts_raw.strip():
        raise AutoTrainConfigError("label.artifacts_dir must be a non-empty string")

    sample_rate = raw.get("sample_rate", 3.0)
    if isinstance(sample_rate, bool) or not isinstance(sample_rate, (int, float)):
        raise AutoTrainConfigError("label.sample_rate must be a positive number")
    sample_rate_f = float(sample_rate)
    if sample_rate_f <= 0.0:
        raise AutoTrainConfigError("label.sample_rate must be a positive number")

    max_frames = raw.get("max_frames", 0)
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames < 0:
        raise AutoTrainConfigError("label.max_frames must be a non-negative int")

    return LabelConfig(
        enabled=bool(raw.get("enabled", True)),
        bootstrap=bool(raw.get("bootstrap", True)),
        conf=_parse_conf(raw.get("conf"), "label.conf", 0.25),
        bootstrap_splits=_parse_bootstrap_splits(raw.get("bootstrap_splits")),
        min_label_ratio=ratio,
        required=bool(raw.get("required", False)),
        teacher=teacher,
        detector=_optional_path(raw.get("detector"), "label.detector"),
        detector_manifest=_optional_path(raw.get("detector_manifest"), "label.detector_manifest"),
        encoder=_optional_path(raw.get("encoder"), "label.encoder"),
        decoder=_optional_path(raw.get("decoder"), "label.decoder"),
        artifacts_dir=Path(artifacts_raw.strip()),
        edgesam_confidence=_parse_conf(
            raw.get("edgesam_confidence"), "label.edgesam_confidence", 0.4
        ),
        sample_rate=sample_rate_f,
        max_frames=max_frames,
        collapse_to_player=bool(raw.get("collapse_to_player", True)),
        keep_negatives=bool(raw.get("keep_negatives", True)),
        coco_fallback=bool(raw.get("coco_fallback", True)),
        download_edgesam=bool(raw.get("download_edgesam", False)),
    )


def _parse_eval(raw: Mapping[str, Any]) -> EvalConfig:
    split = raw.get("split", "val")
    if not isinstance(split, str) or not split.strip():
        raise AutoTrainConfigError("eval.split must be a non-empty string")
    return EvalConfig(
        enabled=bool(raw.get("enabled", True)),
        required=bool(raw.get("required", False)),
        split=split.strip(),
        conf=_parse_conf(raw.get("conf"), "eval.conf", 0.25),
    )


_VALID_SELF_TRAIN_WRITE_POLICIES = frozenset({"if_absent", "overwrite_pseudo", "overwrite_always"})


def _apply_self_train_autonomous_defaults(
    raw: dict[str, Any],
    *,
    default_iterations: int | None = None,
) -> dict[str, Any]:
    """Mutate a copy of self_train raw mapping for autonomous convenience."""
    out = dict(raw)
    out["enabled"] = True
    out["retrain"] = True
    out["conf_schedule"] = True
    out["use_teacher"] = True
    if "iterations" not in out and default_iterations is not None:
        out["iterations"] = default_iterations
    # Ensure iterations is at least 1 when autonomous is set on self_train.
    if "iterations" not in out:
        out["iterations"] = max(1, int(out.get("iterations", 1) or 1))
    return out


def _parse_self_train(raw: Mapping[str, Any]) -> SelfTrainConfig:
    # Work on a mutable copy so autonomous can inject defaults before validation.
    data: dict[str, Any] = dict(raw)
    autonomous = bool(data.get("autonomous", False))
    if autonomous:
        data = _apply_self_train_autonomous_defaults(data)

    write_policy = data.get("write_policy", "overwrite_pseudo")
    if not isinstance(write_policy, str) or write_policy not in _VALID_SELF_TRAIN_WRITE_POLICIES:
        raise AutoTrainConfigError(
            f"self_train.write_policy must be one of {sorted(_VALID_SELF_TRAIN_WRITE_POLICIES)}"
        )
    max_frames = data.get("max_frames", 0)
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames < 0:
        raise AutoTrainConfigError("self_train.max_frames must be a non-negative int")
    conf_threshold = _parse_conf(data.get("conf_threshold"), "self_train.conf_threshold", 0.5)
    conf_low = _optional_float(data.get("conf_low"), "self_train.conf_low")
    conf_high = _optional_float(data.get("conf_high"), "self_train.conf_high")
    if conf_low is not None and not 0.0 <= conf_low <= 1.0:
        raise AutoTrainConfigError("self_train.conf_low must be in [0, 1]")
    if conf_high is not None and not 0.0 <= conf_high <= 1.0:
        raise AutoTrainConfigError("self_train.conf_high must be in [0, 1]")
    if conf_low is not None and conf_high is not None and conf_low > conf_high:
        raise AutoTrainConfigError("self_train.conf_low must be <= conf_high")
    teacher_min_iou = data.get("teacher_min_iou", 0.3)
    try:
        teacher_min_iou_f = float(teacher_min_iou)
    except (TypeError, ValueError) as exc:
        raise AutoTrainConfigError("self_train.teacher_min_iou must be a float in [0, 1]") from exc
    if not 0.0 <= teacher_min_iou_f <= 1.0:
        raise AutoTrainConfigError("self_train.teacher_min_iou must be a float in [0, 1]")
    teacher_model = _optional_path(data.get("teacher_model"), "self_train.teacher_model")
    teacher_manifest = _optional_path(data.get("teacher_manifest"), "self_train.teacher_manifest")
    if (teacher_model is None) ^ (teacher_manifest is None):
        raise AutoTrainConfigError(
            "self_train.teacher_model and teacher_manifest must both be set or both null"
        )

    teacher_strategy_raw = data.get("teacher_strategy", "prev_student")
    if not isinstance(teacher_strategy_raw, str) or not teacher_strategy_raw.strip():
        raise AutoTrainConfigError("self_train.teacher_strategy must be a non-empty string")
    teacher_strategy = teacher_strategy_raw.strip().lower()
    _VALID_TEACHER_STRATEGIES = frozenset({"prev_student", "auto", "best_package", "remote"})
    if teacher_strategy not in _VALID_TEACHER_STRATEGIES:
        raise AutoTrainConfigError(
            "self_train.teacher_strategy must be one of: "
            + ", ".join(sorted(_VALID_TEACHER_STRATEGIES))
        )

    iterations_raw = data.get("iterations", 1)
    if (
        isinstance(iterations_raw, bool)
        or not isinstance(iterations_raw, int)
        or iterations_raw < 1
    ):
        raise AutoTrainConfigError("self_train.iterations must be an integer >= 1")
    iterations = int(iterations_raw)

    max_plateau_raw = data.get("max_plateau_iters", 2)
    if (
        isinstance(max_plateau_raw, bool)
        or not isinstance(max_plateau_raw, int)
        or max_plateau_raw < 1
    ):
        raise AutoTrainConfigError("self_train.max_plateau_iters must be an integer >= 1")
    max_plateau_iters = int(max_plateau_raw)

    enabled = bool(data.get("enabled", False))
    retrain = bool(data.get("retrain", False))
    conf_schedule = bool(data.get("conf_schedule", True))
    stop_on_no_growth = bool(data.get("stop_on_no_growth", True))
    use_teacher = bool(data.get("use_teacher", True))

    # Multi-iter always retrains after accepted labels.
    if iterations > 1:
        if not enabled:
            raise AutoTrainConfigError(
                "self_train.iterations > 1 requires self_train.enabled=true "
                "(or set autonomous=true)"
            )
        retrain = True

    return SelfTrainConfig(
        enabled=enabled,
        required=bool(data.get("required", False)),
        conf_threshold=conf_threshold,
        write_policy=write_policy,
        max_frames=max_frames,
        conf_low=conf_low,
        conf_high=conf_high,
        write_uncertain_queue=bool(data.get("write_uncertain_queue", True)),
        retrain=retrain,
        retrain_required=bool(data.get("retrain_required", False)),
        use_teacher=use_teacher,
        teacher_min_iou=teacher_min_iou_f,
        teacher_model=teacher_model,
        teacher_manifest=teacher_manifest,
        teacher_strategy=teacher_strategy,
        iterations=iterations,
        conf_schedule=conf_schedule,
        stop_on_no_growth=stop_on_no_growth,
        max_plateau_iters=max_plateau_iters,
        autonomous=autonomous,
    )


def _parse_resume(raw: Mapping[str, Any]) -> ResumeConfig:
    return ResumeConfig(enabled=bool(raw.get("enabled", True)))


def _parse_human_gate(raw: Mapping[str, Any]) -> HumanGateConfig:
    message = raw.get(
        "message",
        "human_gate blocked: operator review required before train",
    )
    if not isinstance(message, str) or not message.strip():
        raise AutoTrainConfigError("human_gate.message must be a non-empty string")
    return HumanGateConfig(
        enabled=bool(raw.get("enabled", False)),
        block=bool(raw.get("block", False)),
        message=message.strip(),
    )
