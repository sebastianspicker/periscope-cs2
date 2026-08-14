"""Final auto-train report writing."""

from __future__ import annotations

import json
import os
import tempfile
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class AutoTrainReport:
    """Operator-facing summary written to ``report.json``.

    ``status`` is typically one of:
    ``ok`` | ``degraded`` | ``failed`` | ``human_gate`` | ``pending``.
    """

    schema_version: int = 1
    run_id: str = ""
    mode: str = ""
    status: str = "pending"
    completed_stages: list[str] = field(default_factory=list)
    dataset_root: str | None = None
    onnx_model: str | None = None
    manifest: str | None = None
    package_zip: str | None = None
    progress_report_md: str | None = None
    metrics: dict[str, Any] = field(default_factory=dict)
    eval_metrics: dict[str, Any] | None = None
    self_train_labels_written: int | None = None
    events_jsonl: str | None = None
    retrain_status: str | None = None
    artifacts: dict[str, Any] = field(default_factory=dict)
    notes: list[str] = field(default_factory=list)
    error: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def write_report(path: str | Path, report: AutoTrainReport) -> Path:
    """Atomically write ``report.json``."""
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(report.to_dict(), indent=2, sort_keys=True) + "\n"
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
    return destination


def resolve_report_status(
    state_status: str,
    artifacts: dict[str, Any],
) -> str:
    """Map pipeline state + soft-failure signals to report status.

    * failed / human_gate / pending pass through.
    * completed/running with soft issues → ``degraded``
    * clean completion → ``ok``
    """
    if state_status in {"failed", "human_gate", "pending"}:
        return state_status
    if _has_soft_degradation(artifacts):
        return "degraded"
    if state_status in {"completed", "running"}:
        return "ok"
    return state_status


def _has_soft_degradation(artifacts: dict[str, Any]) -> bool:
    """True when the run finished but with meaningful soft failures."""
    if artifacts.get("smoke_ok") is False:
        return True
    if artifacts.get("smoke_inference") in {"failed", "import_failed"}:
        return True
    if artifacts.get("eval_status") in {"failed", "error"}:
        return True
    if artifacts.get("label_health_warning"):
        return True
    if artifacts.get("empty_label_ratio_warning"):
        return True
    if artifacts.get("label_status") in {
        "bootstrap_failed",
        "bootstrap_insufficient",
        "edgesam_failed",
        "edgesam_insufficient",
        "skipped_edgesam_assets",
    }:
        return True
    soft_notes = artifacts.get("soft_notes")
    if isinstance(soft_notes, list) and soft_notes:
        return True
    degraded_notes = artifacts.get("degraded_notes")
    return bool(isinstance(degraded_notes, list) and degraded_notes)
