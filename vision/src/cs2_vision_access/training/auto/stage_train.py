"""Train stage for auto-train pipelines."""

from __future__ import annotations

from collections.abc import Callable, Mapping
from pathlib import Path
from typing import Any

from cs2_vision_access.training.cloud import (
    create_manifest as cloud_create_manifest,
)
from cs2_vision_access.training.cloud import (
    train as cloud_train,
)
from cs2_vision_access.training.contracts import resolve_train_hyperparameters
from cs2_vision_access.training.local import TrainingSummary, train_and_export
from cs2_vision_access.training.progress_report import (
    find_ultralytics_run_dir,
    parse_ultralytics_results_csv,
    write_progress_report,
)

from .cloud_bundle import materialize_flat_cloud_bundle
from .config import AutoTrainConfig
from .dataset_helpers import (
    count_images_recursive,
    write_session_dataset_yaml,
)
from .errors import AutoTrainStageError
from .notes import append_soft_note
from .paths import RunPaths
from .state import StageState

# Optional injectable callables for unit tests.
TrainLocalFn = Callable[..., TrainingSummary]
TrainCloudFn = Callable[..., Path]


def _should_resume_ultralytics(config: AutoTrainConfig, paths: RunPaths) -> bool:
    if not config.resume.enabled or not config.train.resume_ultralytics:
        return False
    last_pt = paths.train_runs_dir / config.run_id / "weights" / "last.pt"
    return last_pt.is_file()


def _resolve_hyperparameters(config: AutoTrainConfig) -> dict[str, Any]:
    """Fill missing train fields from the named profile; smoke → smoke profile."""
    profile = "smoke" if config.train.smoke else config.train.profile
    # Explicit smoke also forces epochs=1 when the caller did not override.
    epochs = config.train.epochs
    if config.train.smoke and epochs is None:
        epochs = 1
    return resolve_train_hyperparameters(
        profile=profile,
        epochs=epochs,
        batch=config.train.batch,
        image_size=config.train.image_size,
        base_model=config.train.base_model,
    )


def _find_results_csv(paths: RunPaths, config: AutoTrainConfig) -> Path | None:
    run_dir = find_ultralytics_run_dir(paths.train_runs_dir, name=config.run_id)
    if run_dir is None:
        run_dir = find_ultralytics_run_dir(paths.train_runs_dir)
    if run_dir is None:
        return None
    candidate = run_dir / "results.csv"
    return candidate if candidate.is_file() else None


def _write_train_progress(
    config: AutoTrainConfig,
    paths: RunPaths,
    dataset_root: Path,
) -> dict[str, Any]:
    """Write progress report under run_dir/progress; soft-fail on error."""
    updates: dict[str, Any] = {}
    labels_dir = dataset_root / "labels"
    images_dir = dataset_root / "images"
    progress_dir = paths.run_dir / "progress"
    iterations: list[dict[str, Any]] = []
    results_csv = _find_results_csv(paths, config)
    if results_csv is not None:
        metrics = parse_ultralytics_results_csv(results_csv)
        updates["train_metrics"] = metrics
        iter_entry: dict[str, Any] = {
            "iteration": 1,
            "results_csv": str(results_csv),
        }
        results_png = results_csv.with_name("results.png")
        if results_png.is_file():
            iter_entry["results_png"] = str(results_png)
        iterations.append(iter_entry)
    try:
        report_md = write_progress_report(
            progress_dir,
            labels_dir=labels_dir,
            class_names=dict(config.train.class_names),
            iterations=iterations,
            title=f"Auto-train progress ({config.run_id})",
            images_count=count_images_recursive(images_dir) if images_dir.is_dir() else 0,
        )
        updates["progress_report_md"] = str(report_md.resolve())
        updates["progress_dir"] = str(progress_dir.resolve())
    except Exception as error:  # noqa: BLE001 — progress must not fail train
        append_soft_note(updates, f"progress report skipped: {error}")
    return updates


def _check_min_map50(config: AutoTrainConfig, metrics: Mapping[str, Any] | None) -> None:
    threshold = config.train.min_map50
    if threshold is None:
        return
    if not isinstance(metrics, Mapping):
        raise AutoTrainStageError(f"train.min_map50={threshold} set but train metrics are missing")
    map50 = metrics.get("mAP50")
    if map50 is None:
        raise AutoTrainStageError(
            f"train.min_map50={threshold} set but mAP50 missing from train metrics"
        )
    try:
        value = float(map50)
    except (TypeError, ValueError) as error:
        raise AutoTrainStageError(
            f"train.min_map50={threshold} set but mAP50 unparseable: {map50!r}"
        ) from error
    if value < threshold:
        raise AutoTrainStageError(f"train mAP50={value:.4f} below min_map50={threshold:.4f}")


def _resolve_cloud_dataset_root(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> Path:
    """Materialise the flat cloud bundle for ``session_split`` + ``cloud``.

    ``cloud.train`` needs a flat ``images/`` + ``labels/`` tree, so the split
    tree assembled in prepare_data is flattened into ``paths.cloud_dir`` on
    every train. Re-materialising here (rather than in prepare_data) keeps the
    bundle in sync with any labels written by the label stage (edgesam video
    labeling re-assembles the split tree) and with self-train retrains.

    Returns the flat dataset root to hand to the cloud train function.
    """
    dataset_root = state.artifacts.get("dataset_root")
    if not dataset_root:
        raise AutoTrainStageError("train requires dataset_root")
    split_root = Path(str(dataset_root))
    flat_root, zip_path = materialize_flat_cloud_bundle(
        split_root,
        paths.cloud_dir,
        class_names=config.train.class_names,
    )
    flat_root = flat_root.resolve()
    state.artifacts["cloud_dataset_root"] = str(flat_root)
    state.artifacts["cloud_dataset_zip"] = str(zip_path.resolve())
    return flat_root


def stage_train(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    train_local_fn: TrainLocalFn | None = None,
    train_cloud_fn: TrainCloudFn | None = None,
) -> dict[str, Any]:
    """Run local or cloud training (injectable for tests)."""
    dataset_root = state.artifacts.get("dataset_root")
    if not dataset_root:
        raise AutoTrainStageError("train requires dataset_root")
    root = Path(str(dataset_root))
    hp = _resolve_hyperparameters(config)
    epochs = int(hp["epochs"])
    batch = int(hp["batch"])
    image_size = int(hp["image_size"])
    should_resume = _should_resume_ultralytics(config, paths)

    if config.train.backend == "local":
        yaml_path = state.artifacts.get("dataset_yaml")
        if not yaml_path:
            candidate = root / "dataset.yaml"
            if not candidate.is_file():
                candidate = write_session_dataset_yaml(root, config.train.class_names)
            yaml_path = str(candidate)
        train_fn = train_local_fn or train_and_export
        summary = train_fn(
            dataset_yaml=yaml_path,
            dataset_root=root,
            class_names=dict(config.train.class_names),
            base_model=hp["base_model"],
            base_model_origin=config.train.base_model_origin,
            exported_model_license=config.train.exported_model_license,
            allow_model_download=config.train.allow_model_download,
            epochs=epochs,
            image_size=image_size,
            batch=batch,
            device=config.train.device,
            project_directory=paths.train_runs_dir,
            run_name=config.run_id,
            exist_ok=True,
            resume=should_resume,
            lr0=hp.get("lr0"),
            patience=hp.get("patience"),
            plots=True,
        )
        updates: dict[str, Any] = {
            "train_backend": "local",
            "train_profile": hp.get("profile_name"),
            "train_epochs": epochs,
            "train_batch": batch,
            "train_image_size": image_size,
            "train_resume": should_resume,
            "run_directory": summary.run_directory,
            "best_checkpoint": summary.best_checkpoint,
            "onnx_model": summary.onnx_model,
            "manifest": summary.manifest,
        }
        progress = _write_train_progress(config, paths, root)
        updates.update(progress)
        _check_min_map50(config, updates.get("train_metrics"))
        return updates

    # flat_cloud / cloud backend. session_split+cloud trains the flattened
    # cloud bundle (cloud.train's preflight requires a flat images/labels tree).
    train_fn_cloud = train_cloud_fn or cloud_train
    cloud_root = (
        _resolve_cloud_dataset_root(config, paths, state)
        if config.mode == "session_split"
        else root
    )
    onnx_path = train_fn_cloud(
        cloud_root,
        base_model=hp["base_model"],
        epochs=epochs,
        batch=batch,
        imgsz=image_size,
        device=config.train.device,
        classes=dict(config.train.class_names),
        project=paths.train_runs_dir,
        run_name=config.run_id,
        resume=should_resume,
        plots=True,
        lr0=hp.get("lr0"),
        patience=hp.get("patience"),
    )
    manifest_path = cloud_create_manifest(
        onnx_path,
        cloud_root,
        classes=dict(config.train.class_names),
        origin=f"auto-train {config.mode}; base={config.train.base_model_origin}",
        license_name=config.train.exported_model_license,
    )
    updates = {
        "train_backend": "cloud",
        "train_profile": hp.get("profile_name"),
        "train_epochs": epochs,
        "train_batch": batch,
        "train_image_size": image_size,
        "train_resume": should_resume,
        "onnx_model": str(Path(onnx_path).resolve()),
        "manifest": str(Path(manifest_path).resolve()),
    }
    progress = _write_train_progress(config, paths, root)
    updates.update(progress)
    _check_min_map50(config, updates.get("train_metrics"))
    return updates
