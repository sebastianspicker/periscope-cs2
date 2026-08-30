"""Ingest stage for auto-train pipelines."""

from __future__ import annotations

from typing import Any

from .config import AutoTrainConfig
from .dataset_helpers import (
    has_flat_dataset,
    has_split_dataset,
)
from .errors import AutoTrainStageError
from .paths import RunPaths
from .state import StageState


def stage_ingest(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Record / materialise raw inputs for the run."""
    notes: list[str] = []
    updates: dict[str, Any] = {"mode": config.mode}

    if config.mode == "flat_cloud":
        if config.sources.prebuilt_flat_root is not None:
            root = config.sources.prebuilt_flat_root
            if not root.is_dir():
                raise AutoTrainStageError(f"prebuilt_flat_root is not a directory: {root}")
            if not has_flat_dataset(root):
                raise AutoTrainStageError(f"prebuilt_flat_root missing images/ and labels/: {root}")
            updates["dataset_root"] = str(root.resolve())
            updates["ingest_source"] = "prebuilt_flat_root"
            notes.append(f"using prebuilt flat root {root}")
        elif config.sources.dataset_zip is not None:
            zip_path = config.sources.dataset_zip
            if not zip_path.is_file():
                raise AutoTrainStageError(f"dataset_zip not found: {zip_path}")
            # Zip extract runs in prepare_data so ingest stays cheap on resume.
            updates["dataset_zip"] = str(zip_path.resolve())
            updates["ingest_source"] = "dataset_zip"
            notes.append(f"queued zip extract: {zip_path}")
        else:
            raise AutoTrainStageError(
                "flat_cloud requires sources.dataset_zip or sources.prebuilt_flat_root"
            )
    else:
        # session_split
        if config.sources.prebuilt_dataset_root is not None:
            root = config.sources.prebuilt_dataset_root
            if not root.is_dir():
                raise AutoTrainStageError(f"prebuilt_dataset_root is not a directory: {root}")
            if has_split_dataset(root):
                updates["dataset_root"] = str(root.resolve())
                updates["ingest_source"] = "prebuilt_dataset_root"
                notes.append(f"prebuilt split dataset ready: {root}")
            else:
                raise AutoTrainStageError(
                    f"prebuilt_dataset_root must contain images/train (and labels/train): {root}"
                )
        else:
            if config.sources.staging_root is not None:
                staging = config.sources.staging_root
                if not staging.is_dir():
                    raise AutoTrainStageError(f"staging_root is not a directory: {staging}")
                updates["staging_root"] = str(staging.resolve())
                notes.append(f"using external staging_root: {staging}")
            else:
                updates["staging_root"] = str(paths.staging_dir.resolve())
                notes.append(
                    "using run staging dir; provide labels under session folders "
                    "or pass sources.staging_root / prebuilt_dataset_root"
                )
            if config.sources.split_plan is not None:
                plan = config.sources.split_plan
                if not plan.is_file():
                    raise AutoTrainStageError(f"split_plan not found: {plan}")
                updates["split_plan"] = str(plan.resolve())
            elif config.sources.staging_root is not None:
                # Auto whole-session train/val plan when plan not provided.
                try:
                    from cs2_vision_access.workflows.training.session_split import (
                        auto_plan_from_staging,
                    )

                    plan_out = paths.run_dir / "auto_split_plan.json"
                    written = auto_plan_from_staging(config.sources.staging_root, plan_out)
                    updates["split_plan"] = str(written.resolve())
                    notes.append(f"auto-generated session split plan: {written}")
                except ValueError as error:
                    notes.append(f"auto split plan skipped: {error}")
            if config.sources.videos:
                missing = [str(v) for v in config.sources.videos if not v.is_file()]
                if missing:
                    raise AutoTrainStageError(f"video source(s) not found: {missing}")
                updates["videos"] = [str(v.resolve()) for v in config.sources.videos]
                notes.append(
                    "videos listed for frame extract in prepare_data "
                    "(labels still required before validate)"
                )
            if (
                "dataset_root" not in updates
                and "staging_root" not in updates
                and not config.sources.videos
            ):
                raise AutoTrainStageError(
                    "session_split requires prebuilt_dataset_root, "
                    "staging_root (+ split_plan), or videos"
                )
            updates["ingest_source"] = updates.get("ingest_source", "staging_or_videos")

    updates["ingest_notes"] = notes
    state.artifacts.update(updates)
    return updates
