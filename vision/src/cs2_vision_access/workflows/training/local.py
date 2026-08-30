"""Explicit local training and ONNX export path."""

from __future__ import annotations

import json
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cs2_vision_access.application.model_assets.manifest import create_manifest
from cs2_vision_access.workflows.dataset import validate_yolo_segmentation_dataset
from cs2_vision_access.workflows.training.contracts import PROFILES
from cs2_vision_access.workflows.training.train_core import build_train_kwargs


class TrainingError(RuntimeError):
    pass


OFFICIAL_DOWNLOADABLE_BASE_MODELS = frozenset(
    {
        "yolo26n-seg.pt",
        "yolo11n-seg.pt",
    }
)

# Operator-facing artifact layout under the ignored artifacts/ tree.
# Ultralytics writes run plots/weights under project_directory/run_name;
# ONNX + checksum manifest land beside best.pt (weights/), then may be
# promoted by the operator into artifacts/ for outline/benchmark.
DEFAULT_PROJECT_DIRECTORY = Path("artifacts/runs/segment")
DEFAULT_RUN_NAME = "cs2-player"
# Hyperparameter defaults re-exported from contracts.PROFILES (SSOT).
DEFAULT_EPOCHS = PROFILES["local"].epochs
DEFAULT_BATCH = PROFILES["local"].batch
DEFAULT_IMAGE_SIZE = PROFILES["local"].image_size
SMOKE_EPOCHS = PROFILES["smoke"].epochs
SMOKE_BATCH = PROFILES["smoke"].batch


@dataclass(frozen=True)
class TrainingSummary:
    run_directory: str
    best_checkpoint: str
    onnx_model: str
    manifest: str


def resolve_train_hyperparameters(
    *,
    smoke: bool,
    epochs: int | None,
    batch: int | None,
    image_size: int | None = None,
) -> tuple[int, int, int]:
    """Resolve epoch/batch/imgsz, applying few-epoch smoke defaults when requested.

    Explicit non-None values always win. Smoke is a plumbing check only — it does
    not train a CS2-validated model and is not a substitute for a full run.
    """
    resolved_epochs = epochs if epochs is not None else (SMOKE_EPOCHS if smoke else DEFAULT_EPOCHS)
    resolved_batch = batch if batch is not None else (SMOKE_BATCH if smoke else DEFAULT_BATCH)
    resolved_image_size = image_size if image_size is not None else DEFAULT_IMAGE_SIZE
    return resolved_epochs, resolved_batch, resolved_image_size


def _validate_params(
    epochs: int,
    image_size: int,
    batch: int,
    run_name: str,
    base_model_origin: str,
    exported_model_license: str,
    class_names_raw: dict[int, str],
) -> dict[int, str]:
    """Validate training parameters and return normalised class names."""
    if epochs <= 0:
        raise ValueError("epochs must be positive")
    if image_size < 320 or image_size > 1920:
        raise ValueError("image_size must be in [320, 1920]")
    if batch == 0 or batch < -1:
        raise ValueError("batch must be -1 or a positive integer")
    if not run_name.strip():
        raise ValueError("run_name must not be empty")
    _validated_manifest_text(base_model_origin, "--base-model-origin")
    _validated_manifest_text(exported_model_license, "--exported-model-license")
    return _normalise_class_names(class_names_raw)


def _validate_dataset(
    dataset_yaml: str | Path,
    dataset_root: str | Path,
    class_names: dict[int, str],
) -> dict[str, object]:
    """Validate dataset structure and return a normalised YAML config dict."""
    validation = validate_yolo_segmentation_dataset(Path(dataset_root), len(class_names))
    if not validation.is_valid:
        first = validation.issues[0]
        raise TrainingError(
            f"dataset validation failed with {validation.summary.issue_count} issue(s); "
            f"first={first.code}:{first.path}:{first.line or '-'}:{first.message}"
        )

    config = Path(dataset_yaml)
    if (
        config.is_symlink()
        or not config.is_file()
        or config.suffix.lower() not in {".yaml", ".yml"}
    ):
        raise TrainingError("dataset YAML must be a regular local .yaml/.yml file")
    config = config.resolve()
    return _normalise_dataset_yaml_contract(config, Path(dataset_root), class_names)


def _resolve_model_path(
    base_model: str | Path,
    allow_model_download: bool,
) -> str:
    """Resolve a local .pt checkpoint or validate a downloadable model name."""
    base_value = str(base_model)
    base_path = Path(base_value)
    if base_path.is_file():
        if base_path.is_symlink():
            raise TrainingError("base model must not be a symlink")
        if base_path.suffix.lower() != ".pt":
            raise TrainingError("local base model must be a .pt checkpoint")
        return str(base_path.resolve())

    if base_path.exists():
        raise TrainingError("base model must be a regular .pt file")
    if not allow_model_download:
        raise TrainingError(
            "base model is not a local file; pass --allow-model-download to permit "
            "Ultralytics to fetch a named official checkpoint"
        )
    if base_value not in OFFICIAL_DOWNLOADABLE_BASE_MODELS:
        allowed = ", ".join(sorted(OFFICIAL_DOWNLOADABLE_BASE_MODELS))
        raise TrainingError(
            f"downloadable base model must be one of: {allowed}; "
            "download other checkpoints separately and review them before use"
        )
    return base_value


def _resolve_resume(
    *,
    resume: bool,
    project_directory: str | Path,
    run_name: str,
    exist_ok: bool,
) -> tuple[bool, bool]:
    """Decide Ultralytics ``resume`` / ``exist_ok`` given last.pt presence.

    When ``resume=True`` and ``project/run_name/weights/last.pt`` exists, resume
    into that run (forcing ``exist_ok=True``). If last.pt is missing, fall back
    to training from scratch with ``exist_ok=True`` so re-runs do not fail on a
    pre-existing empty run directory.
    """
    if not resume:
        return False, exist_ok
    last_checkpoint = Path(project_directory) / run_name / "weights" / "last.pt"
    if last_checkpoint.is_file():
        return True, True
    return False, True


def _ultralytics_train_kwargs(
    *,
    data: str,
    epochs: int,
    imgsz: int,
    batch: int,
    device: str,
    project: str,
    name: str,
    exist_ok: bool = True,
    plots: bool = True,
    resume: bool = False,
    lr0: float | None = None,
    patience: int | None = None,
    workers: int | None = None,
) -> dict[str, Any]:
    """Local-path helper around :func:`build_train_kwargs`.

    ``workers`` defaults to ``None`` here (omit; use Ultralytics default) unlike
    the shared helper's cloud-oriented default of ``2``.
    """
    return build_train_kwargs(
        data=data,
        epochs=epochs,
        imgsz=imgsz,
        batch=batch,
        device=device,
        project=project,
        name=name,
        exist_ok=exist_ok,
        plots=plots,
        resume=resume,
        lr0=lr0,
        patience=patience,
        workers=workers,
    )


def _run_training(
    normalised_config: dict[str, object],
    model_path: str,
    epochs: int,
    image_size: int,
    batch: int,
    device: str,
    project_directory: str | Path,
    run_name: str,
    *,
    exist_ok: bool = True,
    resume: bool = False,
    lr0: float | None = None,
    patience: int | None = None,
    plots: bool = True,
    workers: int | None = None,
) -> tuple[Path, Path]:
    """Execute Ultralytics training and export to ONNX.

    ``exist_ok`` defaults to True for resume-friendly re-runs into the same
    ``project/run_name`` directory. When ``resume=True``, training continues from
    ``weights/last.pt`` if present; otherwise training starts from scratch with
    ``exist_ok=True``.

    Returns ``(run_directory, exported_onnx_path)``.
    """
    try:
        import onnx  # noqa: F401
    except ImportError as error:
        raise TrainingError(
            "ONNX export support is missing; install the project training extra"
        ) from error

    try:
        from ultralytics import YOLO
    except ImportError as error:
        raise TrainingError(
            "Ultralytics is required for training; install project dependencies"
        ) from error

    effective_resume, effective_exist_ok = _resolve_resume(
        resume=resume,
        project_directory=project_directory,
        run_name=run_name,
        exist_ok=exist_ok,
    )

    try:
        with tempfile.TemporaryDirectory(prefix="cs2-vision-dataset-") as temp_directory:
            training_config = Path(temp_directory) / "dataset.yaml"
            training_config.write_text(
                json.dumps(normalised_config, indent=2) + "\n", encoding="utf-8"
            )
            model = YOLO(model_path, task="segment")
            train_kwargs = _ultralytics_train_kwargs(
                data=str(training_config),
                epochs=epochs,
                imgsz=image_size,
                batch=batch,
                device=device,
                project=str(Path(project_directory)),
                name=run_name,
                exist_ok=effective_exist_ok,
                plots=plots,
                resume=effective_resume,
                lr0=lr0,
                patience=patience,
                workers=workers,
            )
            result: Any = model.train(**train_kwargs)
        run_directory = Path(result.save_dir).resolve()
        best_checkpoint = run_directory / "weights" / "best.pt"
        if not best_checkpoint.is_file():
            raise TrainingError(f"training finished without expected checkpoint: {best_checkpoint}")
        trained = YOLO(str(best_checkpoint), task="segment")
        exported = Path(
            trained.export(format="onnx", imgsz=image_size, dynamic=False, simplify=False)
        ).resolve()
    except TrainingError:
        raise
    except Exception as error:
        raise TrainingError(f"training or ONNX export failed: {error}") from error

    return run_directory, exported


def train_and_export(
    *,
    dataset_yaml: str | Path,
    dataset_root: str | Path,
    class_names: dict[int, str],
    base_model: str | Path,
    base_model_origin: str,
    exported_model_license: str,
    allow_model_download: bool,
    epochs: int,
    image_size: int,
    batch: int,
    device: str,
    project_directory: str | Path,
    run_name: str,
    exist_ok: bool = True,
    resume: bool = False,
    lr0: float | None = None,
    patience: int | None = None,
    plots: bool = True,
    workers: int | None = None,
) -> TrainingSummary:
    """Fine-tune a trusted base checkpoint, then export a checksum-bound ONNX model.

    Extra Ultralytics knobs (``exist_ok``, ``resume``, ``lr0``, ``patience``,
    ``plots``, ``workers``) default to resume-friendly values. ``exist_ok``
    defaults to True so auto-train re-runs can reuse the same run directory.
    """
    class_names = _validate_params(
        epochs,
        image_size,
        batch,
        run_name,
        base_model_origin,
        exported_model_license,
        class_names,
    )
    normalised_config = _validate_dataset(dataset_yaml, dataset_root, class_names)
    model_path = _resolve_model_path(base_model, allow_model_download)
    run_directory, exported = _run_training(
        normalised_config,
        model_path,
        epochs,
        image_size,
        batch,
        device,
        project_directory,
        run_name,
        exist_ok=exist_ok,
        resume=resume,
        lr0=lr0,
        patience=patience,
        plots=plots,
        workers=workers,
    )

    best_checkpoint = run_directory / "weights" / "best.pt"
    manifest_path = exported.with_suffix(".model.json")
    create_manifest(
        exported,
        manifest_path,
        classes=class_names,
        origin=f"local-training; base={base_model_origin}",
        license_name=exported_model_license,
    )
    return TrainingSummary(
        run_directory=str(run_directory),
        best_checkpoint=str(best_checkpoint),
        onnx_model=str(exported),
        manifest=str(manifest_path),
    )


def _normalise_dataset_yaml_contract(
    config: Path,
    dataset_root: Path,
    class_names: dict[int, str],
) -> dict[str, object]:
    """Ensure validation, training, and the exported manifest describe one dataset."""
    try:
        import yaml
    except ImportError as error:  # pragma: no cover - dependency boundary
        raise TrainingError("PyYAML is required to verify the dataset configuration") from error
    try:
        payload = yaml.safe_load(config.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, yaml.YAMLError) as error:
        raise TrainingError(f"could not read dataset YAML: {error}") from error
    if not isinstance(payload, dict):
        raise TrainingError("dataset YAML root must be a mapping")

    configured_names = _normalise_class_names(payload.get("names"))
    if configured_names != class_names:
        raise TrainingError(
            f"dataset YAML classes {configured_names} do not match CLI classes {class_names}"
        )

    configured_root = payload.get("path")
    if not isinstance(configured_root, str) or not configured_root.strip():
        raise TrainingError("dataset YAML path must be a non-empty local path")
    yaml_root = Path(configured_root)
    if not yaml_root.is_absolute():
        yaml_root = config.parent / yaml_root
    if yaml_root.resolve() != dataset_root.resolve():
        raise TrainingError(
            f"dataset YAML path resolves to {yaml_root.resolve()}, but --dataset-root "
            f"resolves to {dataset_root.resolve()}"
        )

    expected_splits = {"train": "images/train", "val": "images/val"}
    for split, expected in expected_splits.items():
        if payload.get(split) != expected:
            raise TrainingError(f"dataset YAML {split} must be {expected!r}")
    test_split = payload.get("test")
    if test_split not in (None, "", "images/test"):
        raise TrainingError("dataset YAML test must be empty or 'images/test'")
    if test_split == "images/test":
        missing_test_directories = [
            str(path)
            for path in (
                dataset_root / "images" / "test",
                dataset_root / "labels" / "test",
            )
            if not path.is_dir() or path.is_symlink()
        ]
        if missing_test_directories:
            raise TrainingError(
                "dataset YAML requests a test split, but required directories are "
                f"missing or unsafe: {missing_test_directories}"
            )

    normalised: dict[str, object] = {
        "path": str(dataset_root.resolve()),
        "train": expected_splits["train"],
        "val": expected_splits["val"],
        "names": [class_names[index] for index in range(len(class_names))],
    }
    if test_split == "images/test":
        normalised["test"] = test_split
    return normalised


def _normalise_class_names(raw: object) -> dict[int, str]:
    values: dict[int, object]
    if isinstance(raw, list):
        values = {index: value for index, value in enumerate(raw)}
    elif isinstance(raw, dict):
        values = {}
        for key, value in raw.items():
            if isinstance(key, bool):
                raise TrainingError("dataset YAML class ids must be integers")
            if isinstance(key, int):
                class_id = key
            elif isinstance(key, str) and key.isascii() and key.isdecimal():
                class_id = int(key)
            else:
                raise TrainingError("dataset YAML class ids must be non-negative integers")
            values[class_id] = value
    else:
        raise TrainingError("dataset YAML names must be a list or mapping")

    if set(values) != set(range(len(values))):
        raise TrainingError("dataset YAML class ids must be contiguous and start at zero")
    if any(not isinstance(value, str) or not value.strip() for value in values.values()):
        raise TrainingError("dataset YAML class names must be non-empty strings")
    normalised = {key: str(value).strip() for key, value in sorted(values.items())}
    if len({value.casefold() for value in normalised.values()}) != len(normalised):
        raise TrainingError("dataset YAML class names must be unique ignoring case")
    return normalised


def _validated_manifest_text(value: str, option_name: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{option_name} must be text")
    normalised = value.strip()
    if not normalised or len(normalised) > 500 or "\n" in normalised:
        raise ValueError(f"{option_name} must be one non-empty line of at most 500 characters")
    return normalised
