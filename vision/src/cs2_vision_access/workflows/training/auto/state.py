"""Atomic stage state persistence for auto-train resume."""

from __future__ import annotations

import json
import os
import tempfile
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

STAGE_ORDER: tuple[str, ...] = (
    "ingest",
    "prepare_data",
    "label",
    "validate",
    "train",
    "export",
    "self_train",
    "eval",
    "report",
)

# Artifact keys produced primarily by each stage. Used to scrub stale keys when
# re-running from ``from_stage`` / ``force`` / resume-disabled.
STAGE_ARTIFACT_KEYS: dict[str, frozenset[str]] = {
    "ingest": frozenset(
        {
            "mode",
            "ingest_source",
            "ingest_notes",
            "dataset_zip",
            "videos",
            "staging_root",
            "split_plan",
            # dataset_root may also be set by prepare_data; re-run of prepare
            # rewrites it. Include here so force/from ingest clears it.
            "dataset_root",
        }
    ),
    "prepare_data": frozenset(
        {
            "dataset_root",
            "dataset_yaml",
            "assemble_summary",
            "prepare_note",
            "staging_root",
        }
    ),
    "label": frozenset(
        {
            "label_enabled",
            "label_status",
            "label_count",
            "label_count_before",
            "label_notes",
            "labels_written",
            "image_count",
            "label_teacher",
            "edgesam_labeled",
            "edgesam_notes",
            "edgesam_assets",
            "bootstrap_splits",
        }
    ),
    "validate": frozenset(
        {
            "validation",
            "label_health",
            "label_health_error",
            "label_health_warning",
            "empty_label_ratio_warning",
            "label_class_id_warnings",
            "train_split",
            "val_split",
            "leaky_val",
        }
    ),
    "train": frozenset(
        {
            "train_backend",
            "train_profile",
            "train_epochs",
            "train_batch",
            "train_image_size",
            "train_resume",
            "run_directory",
            "best_checkpoint",
            "onnx_model",
            "manifest",
            "train_metrics",
            "progress_report_md",
            "progress_dir",
        }
    ),
    "export": frozenset(
        {
            "promoted_onnx",
            "promoted_manifest",
            "onnx_model",
            "manifest",
            "package_zip",
            "smoke_inference",
            "smoke_ok",
            "smoke_source",
        }
    ),
    "self_train": frozenset(
        {
            "self_train_enabled",
            "self_train_status",
            "self_train_report",
            "self_train_labels_written",
            "self_train_accepted",
            "self_train_notes",
            "self_train_history",
            "self_train_iterations_completed",
            "self_train_stopped_reason",
            "uncertain_queue_path",
            "self_train_error",
            "retrain_status",
            "retrain_onnx",
            "retrain_manifest",
            "retrain_error",
            # Multi-iter retrain may refresh these from train/export.
            "onnx_model",
            "manifest",
            "train_metrics",
            "progress_report_md",
            "progress_dir",
            "promoted_onnx",
            "promoted_manifest",
            "package_zip",
            "smoke_inference",
            "smoke_ok",
            "smoke_source",
            "run_directory",
            "best_checkpoint",
            "train_backend",
            "train_profile",
            "train_epochs",
            "train_batch",
            "train_image_size",
            "train_resume",
        }
    ),
    "eval": frozenset(
        {
            "eval_enabled",
            "eval_status",
            "eval_predictions",
            "eval_metrics",
            "eval_metrics_path",
            "eval_notes",
            "eval_error",
        }
    ),
    "report": frozenset(
        {
            "report_path",
            "report_status",
        }
    ),
}

# Cross-cutting soft-note key: drop when any stage that may append is re-run.
_SOFT_NOTE_STAGES = frozenset({"label", "validate", "train", "export", "self_train", "eval"})


class AutoTrainStateError(RuntimeError):
    """state.json is missing, corrupt, or inconsistent."""


@dataclass
class StageState:
    """Per-run stage progress and light metadata."""

    schema_version: int = 1
    run_id: str = ""
    mode: str = ""
    completed_stages: list[str] = field(default_factory=list)
    current_stage: str | None = None
    status: str = "pending"  # pending | running | completed | failed | human_gate
    artifacts: dict[str, Any] = field(default_factory=dict)
    last_error: str | None = None

    def mark_started(self, stage: str) -> None:
        self.current_stage = stage
        self.status = "running"
        self.last_error = None

    def mark_completed(self, stage: str, *, updates: dict[str, Any] | None = None) -> None:
        if stage not in self.completed_stages:
            self.completed_stages.append(stage)
        if updates:
            self.artifacts.update(updates)
        self.current_stage = None
        if stage == STAGE_ORDER[-1]:
            self.status = "completed"

    def mark_failed(self, stage: str, message: str) -> None:
        self.current_stage = stage
        self.status = "failed"
        self.last_error = message

    def mark_human_gate(self, message: str) -> None:
        self.status = "human_gate"
        self.last_error = message

    def is_complete(self, stage: str) -> bool:
        return stage in self.completed_stages

    def scrub_artifacts_for_stages(self, stages: list[str] | tuple[str, ...]) -> None:
        """Drop artifact keys belonging to stages that will re-run."""
        if not stages:
            return
        keys_to_drop: set[str] = set()
        for stage in stages:
            keys_to_drop |= STAGE_ARTIFACT_KEYS.get(stage, frozenset())
        if _SOFT_NOTE_STAGES.intersection(stages):
            keys_to_drop.add("soft_notes")
            keys_to_drop.add("degraded_notes")
        for key in keys_to_drop:
            self.artifacts.pop(key, None)

    def stages_to_run(
        self,
        *,
        force: bool = False,
        from_stage: str | None = None,
    ) -> list[str]:
        """Return ordered stages still needed for this run."""
        if from_stage is not None:
            if from_stage not in STAGE_ORDER:
                raise AutoTrainStateError(
                    f"unknown stage {from_stage!r}; expected one of {list(STAGE_ORDER)}"
                )
            start = STAGE_ORDER.index(from_stage)
            # Invalidate from from_stage onward when resuming from a mid-point.
            keep = [s for s in self.completed_stages if STAGE_ORDER.index(s) < start]
            self.completed_stages = keep
            stages = list(STAGE_ORDER[start:])
            self.scrub_artifacts_for_stages(stages)
            return stages
        if force:
            self.completed_stages = []
            stages = list(STAGE_ORDER)
            self.scrub_artifacts_for_stages(stages)
            return stages
        return [s for s in STAGE_ORDER if s not in self.completed_stages]


def load_state(path: str | Path) -> StageState | None:
    """Load state.json if present; return None when missing."""
    candidate = Path(path)
    if not candidate.exists():
        return None
    if candidate.is_symlink() or not candidate.is_file():
        raise AutoTrainStateError(f"state path is not a regular file: {candidate}")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AutoTrainStateError(f"could not read state.json: {error}") from error
    if not isinstance(payload, dict):
        raise AutoTrainStateError("state.json root must be an object")
    completed = payload.get("completed_stages", [])
    if not isinstance(completed, list) or any(not isinstance(s, str) for s in completed):
        raise AutoTrainStateError("completed_stages must be a list of strings")
    artifacts = payload.get("artifacts", {})
    if not isinstance(artifacts, dict):
        raise AutoTrainStateError("artifacts must be an object")
    return StageState(
        schema_version=int(payload.get("schema_version", 1)),
        run_id=str(payload.get("run_id", "")),
        mode=str(payload.get("mode", "")),
        completed_stages=list(completed),
        current_stage=payload.get("current_stage"),
        status=str(payload.get("status", "pending")),
        artifacts=dict(artifacts),
        last_error=payload.get("last_error"),
    )


def save_state(path: str | Path, state: StageState) -> None:
    """Atomically write state.json via temp file + os.replace."""
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    payload = asdict(state)
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, destination)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
