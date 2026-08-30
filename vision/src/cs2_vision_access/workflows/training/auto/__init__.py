"""Config-driven multi-stage auto-train orchestration.

Stages: ingest → prepare_data → label → validate → train → export → self_train → eval → report.

Modes:
  * ``session_split`` — whole-session YOLO splits; local ``train_and_export``
    or a flattened cloud bundle for ``train.backend=cloud``
  * ``flat_cloud`` — flat images/labels zip + cloud train path
"""

from __future__ import annotations

import logging
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.auto.cloud_bundle import materialize_flat_cloud_bundle
from cs2_vision_access.workflows.training.auto.config import (
    AutoTrainConfig,
    AutoTrainConfigError,
    load_config,
)
from cs2_vision_access.workflows.training.auto.events import append_event
from cs2_vision_access.workflows.training.auto.paths import RunPaths, build_run_paths
from cs2_vision_access.workflows.training.auto.report import AutoTrainReport, resolve_report_status
from cs2_vision_access.workflows.training.auto.stages import (
    AutoTrainStageError,
    ExtractDatasetFn,
    ExtractFramesFn,
    HumanGateBlocked,
    TrainCloudFn,
    TrainLocalFn,
    snapshot_config,
    stage_eval,
    stage_export,
    stage_human_gate,
    stage_ingest,
    stage_label,
    stage_prepare_data,
    stage_report,
    stage_self_train,
    stage_train,
    stage_validate,
)
from cs2_vision_access.workflows.training.auto.state import (
    STAGE_ORDER,
    AutoTrainStateError,
    StageState,
    load_state,
    save_state,
)

LOGGER = logging.getLogger(__name__)

# Exit codes for the CLI / ``run_auto_train``.
EXIT_OK = 0
EXIT_FAIL = 1
EXIT_HUMAN_GATE = 2


class AutoTrainError(RuntimeError):
    """Top-level auto-train failure (maps to exit code 1)."""


@dataclass(frozen=True)
class AutoTrainResult:
    """Outcome of :func:`run_auto_train`."""

    exit_code: int
    run_dir: Path
    state: StageState
    message: str = ""
    report_path: Path | None = None


def run_auto_train(
    config: AutoTrainConfig | str | Path,
    *,
    force: bool = False,
    from_stage: str | None = None,
    train_local_fn: TrainLocalFn | None = None,
    train_cloud_fn: TrainCloudFn | None = None,
    extract_dataset_fn: ExtractDatasetFn | None = None,
    extract_frames_fn: ExtractFramesFn | None = None,
) -> AutoTrainResult:
    """Execute the auto-train stage machine with optional resume.

    Returns an :class:`AutoTrainResult` rather than raising for human-gate
    stops so callers can map exit codes consistently. Unexpected stage
    failures raise :class:`AutoTrainError`.
    """
    if not isinstance(config, AutoTrainConfig):
        try:
            config = load_config(config)
        except AutoTrainConfigError as error:
            raise AutoTrainError(str(error)) from error

    try:
        paths = build_run_paths(config.paths.work_root, config.run_id)
    except ValueError as error:
        raise AutoTrainError(str(error)) from error

    paths.ensure()
    snapshot_config(config, paths)

    state = _load_or_create_state(config, paths, force=force)
    if not config.resume.enabled and not force and from_stage is None:
        # Resume disabled: always start clean unless operator pins from_stage.
        state.completed_stages = []
        state.status = "pending"
        state.last_error = None
        state.scrub_artifacts_for_stages(list(STAGE_ORDER))

    try:
        stages = state.stages_to_run(force=force, from_stage=from_stage)
    except AutoTrainStateError as error:
        raise AutoTrainError(str(error)) from error

    stage_runners: dict[str, Callable[[], dict[str, Any]]] = {
        "ingest": lambda: stage_ingest(config, paths, state),
        "prepare_data": lambda: stage_prepare_data(
            config,
            paths,
            state,
            extract_dataset_fn=extract_dataset_fn,
            extract_frames_fn=extract_frames_fn,
        ),
        "label": lambda: stage_label(config, paths, state),
        "validate": lambda: stage_validate(config, paths, state),
        "train": lambda: stage_train(
            config,
            paths,
            state,
            train_local_fn=train_local_fn,
            train_cloud_fn=train_cloud_fn,
        ),
        "export": lambda: stage_export(config, paths, state),
        "self_train": lambda: stage_self_train(
            config,
            paths,
            state,
            train_local_fn=train_local_fn,
            train_cloud_fn=train_cloud_fn,
        ),
        "eval": lambda: stage_eval(config, paths, state),
        "report": lambda: stage_report(config, paths, state),
    }

    try:
        for stage in stages:
            if stage not in stage_runners:
                raise AutoTrainError(f"unknown stage: {stage}")

            # Human gate sits between validate and train — check before starting.
            if stage == "train":
                try:
                    stage_human_gate(config, state, paths)
                except HumanGateBlocked as gate:
                    state.mark_human_gate(gate.message)
                    save_state(paths.state_path, state)
                    append_event(
                        paths.run_dir,
                        "human_gate",
                        stage="train",
                        status="human_gate",
                        message=gate.message,
                    )
                    _note_events_path(paths, state)
                    _write_partial_report(config, paths, state, status="human_gate")
                    return AutoTrainResult(
                        exit_code=EXIT_HUMAN_GATE,
                        run_dir=paths.run_dir,
                        state=state,
                        message=gate.message,
                        report_path=paths.report_path if paths.report_path.is_file() else None,
                    )

            started = time.perf_counter()
            append_event(paths.run_dir, "stage_start", stage=stage)
            state.mark_started(stage)
            save_state(paths.state_path, state)

            try:
                updates = stage_runners[stage]()
            except HumanGateBlocked:
                raise
            except AutoTrainStageError as error:
                duration_s = round(time.perf_counter() - started, 4)
                append_event(
                    paths.run_dir,
                    "stage_error",
                    stage=stage,
                    duration_s=duration_s,
                    status="failed",
                    message=str(error),
                )
                state.mark_failed(stage, str(error))
                _note_events_path(paths, state)
                save_state(paths.state_path, state)
                _write_partial_report(config, paths, state, status="failed")
                raise AutoTrainError(str(error)) from error
            except Exception as error:
                duration_s = round(time.perf_counter() - started, 4)
                append_event(
                    paths.run_dir,
                    "stage_error",
                    stage=stage,
                    duration_s=duration_s,
                    status="failed",
                    message=str(error),
                )
                state.mark_failed(stage, str(error))
                _note_events_path(paths, state)
                save_state(paths.state_path, state)
                _write_partial_report(config, paths, state, status="failed")
                raise AutoTrainError(f"{stage} failed: {error}") from error

            duration_s = round(time.perf_counter() - started, 4)
            append_event(
                paths.run_dir,
                "stage_end",
                stage=stage,
                duration_s=duration_s,
                status="ok",
            )
            state.mark_completed(stage, updates=updates)
            if stage == STAGE_ORDER[-1]:
                state.status = "completed"
            _note_events_path(paths, state)
            save_state(paths.state_path, state)

    except AutoTrainError:
        raise
    except HumanGateBlocked as gate:
        state.mark_human_gate(gate.message)
        append_event(
            paths.run_dir,
            "human_gate",
            status="human_gate",
            message=gate.message,
        )
        _note_events_path(paths, state)
        save_state(paths.state_path, state)
        return AutoTrainResult(
            exit_code=EXIT_HUMAN_GATE,
            run_dir=paths.run_dir,
            state=state,
            message=gate.message,
        )

    append_event(
        paths.run_dir,
        "complete",
        status=state.status,
        completed_stages=list(state.completed_stages),
    )
    _note_events_path(paths, state)
    save_state(paths.state_path, state)

    return AutoTrainResult(
        exit_code=EXIT_OK,
        run_dir=paths.run_dir,
        state=state,
        message="ok",
        report_path=paths.report_path if paths.report_path.is_file() else None,
    )


def _note_events_path(paths: RunPaths, state: StageState) -> None:
    """Soft-path: surface events.jsonl on state artifacts when present."""
    try:
        events_path = paths.run_dir / "events.jsonl"
        if events_path.is_file():
            state.artifacts["events_jsonl"] = str(events_path.resolve())
    except OSError as error:
        # The event pointer is supplemental; a transient filesystem problem
        # must not turn an otherwise completed training stage into a failure.
        LOGGER.debug("Could not record events.jsonl artifact: %s", error)


def _load_or_create_state(
    config: AutoTrainConfig,
    paths: RunPaths,
    *,
    force: bool,
) -> StageState:
    existing = load_state(paths.state_path)
    if existing is not None and not force:
        if existing.run_id and existing.run_id != config.run_id:
            raise AutoTrainError(
                f"state.json run_id {existing.run_id!r} does not match "
                f"config run_id {config.run_id!r}"
            )
        if existing.mode and existing.mode != config.mode:
            raise AutoTrainError(
                f"state.json mode {existing.mode!r} does not match config mode {config.mode!r}"
            )
        existing.run_id = config.run_id
        existing.mode = config.mode
        return existing
    return StageState(run_id=config.run_id, mode=config.mode)


def _write_partial_report(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    status: str,
) -> None:
    from cs2_vision_access.workflows.training.auto.report import write_report

    report_status = status
    if status not in {"failed", "human_gate"}:
        report_status = resolve_report_status(status, state.artifacts)

    st_written = state.artifacts.get("self_train_labels_written")
    if st_written is not None:
        try:
            st_written = int(st_written)
        except (TypeError, ValueError):
            st_written = None
    report = AutoTrainReport(
        schema_version=1,
        run_id=config.run_id,
        mode=config.mode,
        status=report_status,
        completed_stages=list(state.completed_stages),
        dataset_root=(
            str(state.artifacts["dataset_root"]) if "dataset_root" in state.artifacts else None
        ),
        onnx_model=(
            str(state.artifacts["onnx_model"]) if "onnx_model" in state.artifacts else None
        ),
        manifest=(str(state.artifacts["manifest"]) if "manifest" in state.artifacts else None),
        package_zip=(
            str(state.artifacts["package_zip"]) if "package_zip" in state.artifacts else None
        ),
        progress_report_md=(
            str(state.artifacts["progress_report_md"])
            if "progress_report_md" in state.artifacts
            else None
        ),
        metrics=dict(state.artifacts.get("train_metrics") or {}),
        eval_metrics=(
            state.artifacts.get("eval_metrics")
            if isinstance(state.artifacts.get("eval_metrics"), dict)
            else None
        ),
        self_train_labels_written=st_written,
        events_jsonl=(
            str(state.artifacts["events_jsonl"]) if "events_jsonl" in state.artifacts else None
        ),
        retrain_status=(
            str(state.artifacts["retrain_status"]) if "retrain_status" in state.artifacts else None
        ),
        artifacts=dict(state.artifacts),
        error=state.last_error,
    )
    write_report(paths.report_path, report)


__all__ = [
    "EXIT_FAIL",
    "EXIT_HUMAN_GATE",
    "EXIT_OK",
    "STAGE_ORDER",
    "AutoTrainConfig",
    "AutoTrainConfigError",
    "AutoTrainError",
    "AutoTrainResult",
    "AutoTrainStageError",
    "HumanGateBlocked",
    "RunPaths",
    "StageState",
    "append_event",
    "build_run_paths",
    "load_config",
    "materialize_flat_cloud_bundle",
    "run_auto_train",
]
