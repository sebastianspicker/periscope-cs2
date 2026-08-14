"""Eval stage for auto-train pipelines."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from cs2_vision_access.dataset.types import IMAGE_EXTENSIONS

from .config import AutoTrainConfig
from .errors import AutoTrainStageError
from .notes import append_soft_note
from .paths import RunPaths
from .state import StageState


def stage_eval(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Optional mask eval after export; soft-fail unless eval.required."""
    updates: dict[str, Any] = {"eval_enabled": config.eval.enabled}
    if not config.eval.enabled:
        updates["eval_status"] = "disabled"
        return updates

    onnx_raw = state.artifacts.get("onnx_model")
    manifest_raw = state.artifacts.get("manifest")
    dataset_root = state.artifacts.get("dataset_root")
    if not onnx_raw or not manifest_raw or not dataset_root:
        msg = "eval skipped: missing onnx/manifest/dataset_root"
        updates["eval_status"] = "skipped_missing_artifacts"
        if config.eval.required:
            raise AutoTrainStageError(msg)
        append_soft_note(updates, msg)
        return updates

    root = Path(str(dataset_root))
    split = config.eval.split
    # Flat layouts often lack images/val — fall back to flat/all.
    images_val = root / "images" / split
    images_flat = root / "images"
    if not images_val.is_dir():
        if images_flat.is_dir() and any(
            p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS for p in images_flat.iterdir()
        ):
            split = "flat"
        else:
            msg = f"eval skipped: no images for split={config.eval.split!r}"
            updates["eval_status"] = "skipped_no_images"
            if config.eval.required:
                raise AutoTrainStageError(msg)
            notes = [msg]
            updates["eval_notes"] = notes
            return updates

    preds_path = paths.run_dir / "eval" / "predictions.json"
    metrics_path = paths.run_dir / "eval" / "metrics.json"
    try:
        from cs2_vision_access.training.export_predictions import (
            export_predictions,
            run_eval_masks_if_available,
        )

        export_predictions(
            onnx_raw,
            manifest_raw,
            root,
            split=split,
            output_json=preds_path,
            device=config.train.device,
            conf=config.eval.conf,
        )
        metrics = run_eval_masks_if_available(
            preds_path,
            root,
            split,
            out_json=metrics_path,
        )
        updates["eval_status"] = "ok" if metrics is not None else "preds_only"
        updates["eval_predictions"] = str(preds_path.resolve())
        if metrics is not None:
            updates["eval_metrics"] = metrics
            updates["eval_metrics_path"] = str(metrics_path.resolve())
        else:
            updates["eval_notes"] = ["eval-masks package unavailable; preds only"]
    except Exception as error:
        msg = f"eval failed: {error}"
        updates["eval_status"] = "failed"
        updates["eval_error"] = str(error)
        if config.eval.required:
            raise AutoTrainStageError(msg) from error
        append_soft_note(updates, msg)
        updates["eval_notes"] = [msg]

    return updates
