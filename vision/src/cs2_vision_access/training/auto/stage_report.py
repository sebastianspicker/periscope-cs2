"""Report stage and config snapshot helpers for auto-train pipelines."""

from __future__ import annotations

import json
from typing import Any

from .config import AutoTrainConfig
from .paths import RunPaths
from .state import StageState


def _as_str(value: object) -> str | None:
    if value is None:
        return None
    return str(value)


def snapshot_config(config: AutoTrainConfig, paths: RunPaths) -> None:
    """Persist a JSON snapshot of the resolved config under the run dir."""
    paths.config_snapshot_path.parent.mkdir(parents=True, exist_ok=True)
    paths.config_snapshot_path.write_text(
        json.dumps(config.to_dict(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def stage_report(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Materialise report.json from accumulated artifacts."""
    from .report import (
        AutoTrainReport,
        resolve_report_status,
        write_report,
    )

    artifacts = dict(state.artifacts)
    events_path = paths.run_dir / "events.jsonl"
    if events_path.is_file() and "events_jsonl" not in artifacts:
        artifacts["events_jsonl"] = str(events_path.resolve())
    status = resolve_report_status(
        state.status if state.status != "running" else "completed",
        artifacts,
    )
    notes: list[str] = list(artifacts.get("ingest_notes") or [])
    for key in ("label_notes", "self_train_notes", "eval_notes", "soft_notes"):
        extra = artifacts.get(key) or []
        if isinstance(extra, list):
            notes.extend(str(item) for item in extra)

    metrics: dict[str, Any] = {}
    if isinstance(artifacts.get("train_metrics"), dict):
        metrics.update(artifacts["train_metrics"])
    eval_metrics = artifacts.get("eval_metrics")
    if not isinstance(eval_metrics, dict):
        eval_metrics = None

    self_train_labels_written = artifacts.get("self_train_labels_written")
    if self_train_labels_written is not None:
        try:
            self_train_labels_written = int(self_train_labels_written)
        except (TypeError, ValueError):
            self_train_labels_written = None
    events_jsonl = _as_str(artifacts.get("events_jsonl"))
    retrain_status = _as_str(artifacts.get("retrain_status"))

    report = AutoTrainReport(
        schema_version=1,
        run_id=config.run_id,
        mode=config.mode,
        status=status,
        completed_stages=list(state.completed_stages) + ["report"],
        dataset_root=_as_str(artifacts.get("dataset_root")),
        onnx_model=_as_str(artifacts.get("onnx_model")),
        manifest=_as_str(artifacts.get("manifest")),
        package_zip=_as_str(artifacts.get("package_zip")),
        progress_report_md=_as_str(artifacts.get("progress_report_md")),
        metrics=metrics,
        eval_metrics=eval_metrics,
        self_train_labels_written=self_train_labels_written,
        events_jsonl=events_jsonl,
        retrain_status=retrain_status,
        artifacts={
            k: v
            for k, v in artifacts.items()
            if k
            not in {
                "dataset_root",
                "onnx_model",
                "manifest",
                "package_zip",
                "progress_report_md",
                "train_metrics",
                "eval_metrics",
            }
        },
        notes=notes,
        error=state.last_error,
    )
    write_report(paths.report_path, report)
    return {
        "report_path": str(paths.report_path.resolve()),
        "report_status": status,
    }
