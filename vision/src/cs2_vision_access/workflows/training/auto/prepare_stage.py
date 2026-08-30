"""Prepare-data stage for auto-train pipelines.

Extracted from ``stages.py`` to keep stage runners modular.
"""

from __future__ import annotations

import shutil
from collections.abc import Callable
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.dataset.split import assemble_dataset, load_split_plan
from cs2_vision_access.workflows.training.auto.config import AutoTrainConfig
from cs2_vision_access.workflows.training.auto.dataset_helpers import (
    has_flat_dataset,
    has_split_dataset,
    write_session_dataset_yaml,
)
from cs2_vision_access.workflows.training.auto.errors import AutoTrainStageError
from cs2_vision_access.workflows.training.auto.paths import RunPaths
from cs2_vision_access.workflows.training.auto.state import StageState
from cs2_vision_access.workflows.training.cloud import extract_dataset

ExtractDatasetFn = Callable[..., Path]
ExtractFramesFn = Callable[..., Any]


def stage_prepare_data(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    extract_dataset_fn: ExtractDatasetFn | None = None,
    extract_frames_fn: ExtractFramesFn | None = None,
) -> dict[str, Any]:
    """Resolve a dataset root ready for validation / training."""
    updates: dict[str, Any] = {}

    if config.mode == "flat_cloud":
        existing = state.artifacts.get("dataset_root")
        if existing and Path(str(existing)).is_dir() and has_flat_dataset(Path(str(existing))):
            updates["dataset_root"] = str(Path(str(existing)).resolve())
            return updates

        zip_path = state.artifacts.get("dataset_zip") or (
            str(config.sources.dataset_zip) if config.sources.dataset_zip is not None else None
        )
        if not zip_path:
            raise AutoTrainStageError("flat_cloud prepare_data has no dataset source")
        extract = extract_dataset_fn or extract_dataset
        if paths.extracted_dir.exists():
            shutil.rmtree(paths.extracted_dir)
        paths.extracted_dir.mkdir(parents=True, exist_ok=True)
        data_root = extract(zip_path, output_dir=paths.extracted_dir)
        updates["dataset_root"] = str(Path(data_root).resolve())
        return updates

    # session_split
    existing = state.artifacts.get("dataset_root")
    if existing and has_split_dataset(Path(str(existing))):
        root = Path(str(existing))
        yaml_path = root / "dataset.yaml"
        if not yaml_path.is_file():
            yaml_path = write_session_dataset_yaml(root, config.train.class_names)
        updates["dataset_root"] = str(root.resolve())
        updates["dataset_yaml"] = str(yaml_path.resolve())
        return updates

    staging_root = Path(str(state.artifacts.get("staging_root") or paths.staging_dir))
    videos = state.artifacts.get("videos") or [str(v) for v in config.sources.videos]
    if videos:
        try:
            from cs2_vision_access.workflows.dataset.frames import (
                extract_frames as default_extract_frames,
            )
        except ImportError as error:
            raise AutoTrainStageError(f"frame extraction unavailable: {error}") from error
        extract_frames = extract_frames_fn or default_extract_frames
        for video in videos:
            video_path = Path(str(video))
            session_id = video_path.stem
            session_dir = staging_root / session_id
            if session_dir.exists() and any(session_dir.iterdir()):
                continue
            session_dir.mkdir(parents=True, exist_ok=True)
            # extract_frames requires empty dir — recreate cleanly
            if any(session_dir.iterdir()):
                shutil.rmtree(session_dir)
                session_dir.mkdir(parents=True, exist_ok=True)
            extract_frames(
                video_path,
                session_dir,
                every_n_frames=config.sources.every_n_frames,
                max_saved_frames=config.sources.max_saved_frames,
                session_id=session_id,
            )
        updates["staging_root"] = str(staging_root.resolve())
        updates["prepare_note"] = (
            "frames extracted into staging; YOLO labels must already exist "
            "(or be added) under each session before assemble/validate"
        )

    plan_path = state.artifacts.get("split_plan") or (
        str(config.sources.split_plan) if config.sources.split_plan is not None else None
    )
    if plan_path:
        plan = load_split_plan(plan_path)
        if paths.dataset_dir.exists():
            # assemble_dataset refuses non-empty unless overwrite
            for child in ("images", "labels", "sessions.json", "dataset.yaml"):
                target = paths.dataset_dir / child
                if target.is_dir():
                    shutil.rmtree(target)
                elif target.is_file():
                    target.unlink()
        summary = assemble_dataset(
            staging_root,
            paths.dataset_dir,
            plan,
            overwrite=True,
        )
        yaml_path = write_session_dataset_yaml(paths.dataset_dir, config.train.class_names)
        updates["dataset_root"] = str(paths.dataset_dir.resolve())
        updates["dataset_yaml"] = str(yaml_path.resolve())
        updates["assemble_summary"] = summary.as_dict()
        return updates

    # No plan: if staging itself looks like a split dataset, use it; else fail
    # with a clear MVP path.
    if has_split_dataset(staging_root):
        yaml_path = staging_root / "dataset.yaml"
        if not yaml_path.is_file():
            yaml_path = write_session_dataset_yaml(staging_root, config.train.class_names)
        updates["dataset_root"] = str(staging_root.resolve())
        updates["dataset_yaml"] = str(yaml_path.resolve())
        return updates

    raise AutoTrainStageError(
        "session_split prepare_data could not assemble a dataset. "
        "Provide sources.prebuilt_dataset_root with images/train, or "
        "sources.staging_root + sources.split_plan for assemble_dataset. "
        "Video-only extract produces frames without labels."
    )
