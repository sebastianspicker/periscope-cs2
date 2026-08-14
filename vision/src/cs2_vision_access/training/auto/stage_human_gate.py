"""Human gate stage for auto-train pipelines."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .config import AutoTrainConfig
from .errors import HumanGateBlocked
from .paths import RunPaths
from .state import StageState


def _resolve_uncertain_review_path(paths: RunPaths) -> Path | None:
    """Return the first existing uncertain-review JSON under the run dir."""
    candidates = (
        paths.run_dir / "progress" / "uncertain_review.json",
        paths.run_dir / "uncertain_review.json",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


def _write_human_gate_report(
    *,
    paths: RunPaths,
    state: StageState,
    config: AutoTrainConfig,
    message: str,
    uncertain_review: Path | None,
    blocked: bool,
) -> Path:
    """Write operator-facing gate snapshot under ``progress/human_gate.json``."""
    progress_dir = paths.run_dir / "progress"
    progress_dir.mkdir(parents=True, exist_ok=True)
    report_path = progress_dir / "human_gate.json"
    payload: dict[str, Any] = {
        "schema_version": 1,
        "run_id": config.run_id,
        "mode": config.mode,
        "blocked": blocked,
        "enabled": config.human_gate.enabled,
        "block": config.human_gate.block,
        "message": message,
        "completed_stages": list(state.completed_stages),
        "artifacts": dict(state.artifacts),
        "uncertain_review": str(uncertain_review) if uncertain_review is not None else None,
        "dataset_root": state.artifacts.get("dataset_root"),
        "dataset_yaml": state.artifacts.get("dataset_yaml"),
        "label_count": state.artifacts.get("label_count"),
        "image_count": state.artifacts.get("image_count"),
    }
    report_path.write_text(
        json.dumps(payload, indent=2, sort_keys=True, default=str) + "\n",
        encoding="utf-8",
    )
    return report_path.resolve()


def stage_human_gate(
    config: AutoTrainConfig,
    state: StageState,
    paths: RunPaths | None = None,
) -> None:
    """Optional operator gate between validate and train.

    When ``human_gate.enabled`` and ``human_gate.block`` are both true, the
    pipeline stops before train (exit code 2). The gate:

    * resolves ``progress/uncertain_review.json`` (or run-dir root) when present
      from a prior review / self-train pass and appends it to the block message;
    * writes ``progress/human_gate.json`` with completed stages, key artifacts,
      and the review path so operators have a concrete review package;
    * raises :class:`HumanGateBlocked` with the enriched message.

    When the gate is disabled, or enabled without ``block``, this is a no-op
    (no report is written).
    """
    if not (config.human_gate.enabled and config.human_gate.block):
        return

    message = config.human_gate.message
    uncertain_review: Path | None = None
    gate_report: Path | None = None

    if paths is not None:
        uncertain_review = _resolve_uncertain_review_path(paths)
        if uncertain_review is not None:
            message = f"{message} | uncertain_review: {uncertain_review}"
        gate_report = _write_human_gate_report(
            paths=paths,
            state=state,
            config=config,
            message=message,
            uncertain_review=uncertain_review,
            blocked=True,
        )
        message = f"{message} | human_gate_report: {gate_report}"

    raise HumanGateBlocked(message)
