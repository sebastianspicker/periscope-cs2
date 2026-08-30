"""Self-train stage for auto-train pipelines.

Extracted from ``stages.py`` to keep stage runners modular. Retrain re-enters
``stage_train`` / ``stage_export`` via lazy imports to avoid circular deps.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.active_learning import rank_uncertain_queue_soft
from cs2_vision_access.workflows.training.auto.config import AutoTrainConfig
from cs2_vision_access.workflows.training.auto.errors import AutoTrainStageError
from cs2_vision_access.workflows.training.auto.notes import (
    append_soft_note,
    apply_merged_soft_notes,
    merge_soft_notes,
)
from cs2_vision_access.workflows.training.auto.paths import RunPaths
from cs2_vision_access.workflows.training.auto.state import StageState
from cs2_vision_access.workflows.training.dataset_zip import resolve_self_train_dirs
from cs2_vision_access.workflows.training.self_train_schedule import (
    growth_plateau_update,
    resolve_conf_bands,
)
from cs2_vision_access.workflows.training.teacher_strategy import get_teacher_strategy


def _run_retrain(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    train_local_fn: Any | None = None,
    train_cloud_fn: Any | None = None,
) -> dict[str, Any]:
    """Re-run train then export on the dataset after self-train labels.

    Does not mutate ``state.artifacts`` permanently; returns combined updates
    for the self_train stage to merge (onnx_model, manifest, etc.).

    Imports the defining stage modules rather than the public aggregation
    facade, preserving a one-way module graph.
    """
    from cs2_vision_access.workflows.training.auto.stage_export import stage_export
    from cs2_vision_access.workflows.training.auto.stage_train import stage_train

    train_updates = stage_train(
        config,
        paths,
        state,
        train_local_fn=train_local_fn,
        train_cloud_fn=train_cloud_fn,
    )
    # Export needs the new onnx/manifest from train without polluting state
    # if retrain later fails soft (self_train still returns its own updates).
    export_state = StageState(
        schema_version=state.schema_version,
        run_id=state.run_id,
        mode=state.mode,
        completed_stages=list(state.completed_stages),
        artifacts={**dict(state.artifacts), **train_updates},
    )
    export_updates = stage_export(config, paths, export_state)
    combined: dict[str, Any] = {**train_updates, **export_updates}
    # Preserve soft_notes from both steps when both present.
    merged_notes = merge_soft_notes(train_updates, export_updates)
    if merged_notes:
        combined["soft_notes"] = merged_notes
    return combined


def stage_self_train(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    train_local_fn: Any | None = None,
    train_cloud_fn: Any | None = None,
) -> dict[str, Any]:
    """Optional post-export pseudo-labeling; soft-fail unless self_train.required.

    Uses student ONNX + manifest from export/train artifacts. Operates on flat
    images/labels or images/train + labels/train only (never val).

    Multi-iter: when ``self_train.iterations > 1`` (or iterations==1 with
    ``retrain``), runs self_train→retrain cycles. Conf may be scheduled upward;
    previous-cycle ONNX becomes teacher when ``use_teacher`` and it > 1.
    Retrain failures are soft unless ``self_train.retrain_required``.
    """
    st_cfg = config.self_train
    updates: dict[str, Any] = {"self_train_enabled": st_cfg.enabled}
    notes: list[str] = []

    if not st_cfg.enabled:
        updates["self_train_status"] = "disabled"
        updates["self_train_iterations_completed"] = 0
        updates["self_train_stopped_reason"] = "disabled"
        updates["self_train_history"] = []
        return updates

    onnx_raw = state.artifacts.get("onnx_model")
    manifest_raw = state.artifacts.get("manifest")
    dataset_root = state.artifacts.get("dataset_root")
    if not onnx_raw or not manifest_raw or not dataset_root:
        msg = "self_train skipped: missing onnx/manifest/dataset_root"
        updates["self_train_status"] = "skipped_missing_artifacts"
        notes.append(msg)
        updates["self_train_notes"] = notes
        updates["self_train_iterations_completed"] = 0
        updates["self_train_stopped_reason"] = "error"
        updates["self_train_history"] = []
        if st_cfg.required:
            raise AutoTrainStageError(msg)
        append_soft_note(updates, msg)
        return updates

    root = Path(str(dataset_root))
    if not root.is_dir():
        msg = f"self_train: dataset_root is not a directory: {root}"
        updates["self_train_status"] = "failed"
        updates["self_train_error"] = msg
        updates["self_train_iterations_completed"] = 0
        updates["self_train_stopped_reason"] = "error"
        updates["self_train_history"] = []
        if st_cfg.required:
            raise AutoTrainStageError(msg)
        notes.append(msg)
        updates["self_train_notes"] = notes
        append_soft_note(updates, msg)
        return updates

    resolved = resolve_self_train_dirs(root)
    if resolved is None:
        msg = (
            "self_train skipped: need flat images/labels or images/train + "
            "labels/train under dataset_root"
        )
        updates["self_train_status"] = "skipped_no_train_split"
        notes.append(msg)
        updates["self_train_notes"] = notes
        updates["self_train_iterations_completed"] = 0
        updates["self_train_stopped_reason"] = "error"
        updates["self_train_history"] = []
        if st_cfg.required:
            raise AutoTrainStageError(msg)
        append_soft_note(updates, msg)
        return updates

    images_dir, labels_dir = resolved
    iterations = max(1, int(st_cfg.iterations))
    # Multi-iter always retrains after accepted labels; single-iter uses flag.
    do_retrain = bool(st_cfg.retrain) or iterations > 1
    base_conf = float(st_cfg.conf_threshold)
    allowed_class_ids = set(config.train.class_names.keys())

    uncertain_path: Path | None = None
    if st_cfg.write_uncertain_queue:
        uncertain_path = paths.run_dir / "uncertain_queue.jsonl"

    working_onnx: Any = onnx_raw
    working_manifest: Any = manifest_raw
    # Explicit teacher only when both paths configured (iter 1).
    explicit_teacher_model = st_cfg.teacher_model
    explicit_teacher_manifest = st_cfg.teacher_manifest
    # Snapshot of the student used in the previous cycle (teacher for it>1).
    prev_student_onnx: Any = None
    prev_student_manifest: Any = None

    history: list[dict[str, Any]] = []
    total_accepted = 0
    plateau = 0
    stopped_reason = "completed"
    last_retrain_status = "disabled"
    last_report_path: Path | None = None

    # Working state mirror for retrain: keeps onnx/manifest current without
    # permanently mutating the caller's state until we return updates.
    working_state = StageState(
        schema_version=state.schema_version,
        run_id=state.run_id,
        mode=state.mode,
        completed_stages=list(state.completed_stages),
        artifacts=dict(state.artifacts),
    )

    try:
        from cs2_vision_access.workflows.training.self_train import run_self_train_iteration
    except Exception as error:  # noqa: BLE001
        msg = f"self_train failed: {error}"
        updates["self_train_status"] = "failed"
        updates["self_train_error"] = str(error)
        notes.append(msg)
        updates["self_train_notes"] = notes
        updates["self_train_history"] = history
        updates["self_train_iterations_completed"] = 0
        updates["self_train_stopped_reason"] = "error"
        if st_cfg.required:
            raise AutoTrainStageError(msg) from error
        append_soft_note(updates, msg)
        return updates

    for it in range(1, iterations + 1):
        conf_threshold, conf_low, conf_high = resolve_conf_bands(
            base_conf,
            conf_low=st_cfg.conf_low,
            conf_high=st_cfg.conf_high,
            iteration=it,
            conf_schedule=bool(st_cfg.conf_schedule),
        )

        # Student for this cycle is the current working onnx (initial export
        # or last retrain). Teacher is the *previous* cycle student so it
        # differs from the current student after retrain.
        student_onnx = working_onnx
        student_manifest = working_manifest

        teacher_model = None
        teacher_manifest = None
        teacher_note: str | None = None
        if st_cfg.use_teacher:
            strategy = get_teacher_strategy(st_cfg.teacher_strategy)
            pair = strategy.resolve(
                iteration=it,
                prev_student_onnx=prev_student_onnx,
                prev_student_manifest=prev_student_manifest,
                explicit_onnx=explicit_teacher_model,
                explicit_manifest=explicit_teacher_manifest,
            )
            if pair is not None:
                teacher_model = pair.model
                teacher_manifest = pair.manifest
                if it > 1 and strategy.name == "prev_student":
                    teacher_note = (
                        f"iter{it}: teacher=prev_student ({Path(str(teacher_model)).name})"
                    )
                else:
                    teacher_note = (
                        f"iter{it}: teacher={Path(str(teacher_model)).name} "
                        f"min_iou={st_cfg.teacher_min_iou}"
                    )
            elif it > 1:
                teacher_note = (
                    f"iter{it}: use_teacher=true but no prior student onnx; "
                    "running without teacher gate"
                )
            else:
                teacher_note = (
                    f"iter{it}: use_teacher=true but no teacher_model/"
                    "teacher_manifest; running without teacher gate"
                )

        if st_cfg.conf_schedule:
            notes.append(
                f"iter{it}: conf_schedule conf={conf_threshold:.4f} (base={base_conf:.4f})"
            )

        if teacher_note:
            notes.append(teacher_note)

        report_path = (
            paths.run_dir / "self_train_report.json"
            if iterations == 1
            else paths.run_dir / f"self_train_report_iter{it:02d}.json"
        )
        last_report_path = report_path

        st_kwargs: dict[str, Any] = dict(
            conf_threshold=conf_threshold,
            conf_low=conf_low,
            conf_high=conf_high,
            device=config.train.device,
            max_frames=st_cfg.max_frames,
            write_policy=st_cfg.write_policy,
            allowed_class_ids=allowed_class_ids,
            uncertain_queue_path=uncertain_path,
            report_path=report_path,
            fail_closed=bool(st_cfg.required),
        )
        if teacher_model is not None and teacher_manifest is not None:
            st_kwargs["teacher_model"] = teacher_model
            st_kwargs["teacher_manifest"] = teacher_manifest
            st_kwargs["teacher_min_iou"] = float(st_cfg.teacher_min_iou)

        cycle: dict[str, Any] = {
            "iteration": it,
            "accepted": 0,
            "conf": conf_threshold,
            "teacher": (str(teacher_model) if teacher_model is not None else None),
            "retrain_status": "not_run",
        }

        try:
            report = run_self_train_iteration(
                images_dir,
                labels_dir,
                student_onnx,
                student_manifest,
                **st_kwargs,
            )
        except Exception as error:  # noqa: BLE001 — soft fail unless required
            msg = f"self_train failed (iter {it}): {error}"
            updates["self_train_status"] = "failed"
            updates["self_train_error"] = str(error)
            notes.append(msg)
            cycle["retrain_status"] = "not_run"
            cycle["error"] = str(error)
            history.append(cycle)
            updates["self_train_history"] = history
            updates["self_train_notes"] = notes
            updates["self_train_iterations_completed"] = it - 1
            updates["self_train_stopped_reason"] = "error"
            updates["self_train_labels_written"] = total_accepted
            updates["self_train_accepted"] = total_accepted
            if st_cfg.required:
                raise AutoTrainStageError(msg) from error
            append_soft_note(updates, msg)
            return updates

        written_count = int(report.accepted)
        total_accepted += written_count
        cycle["accepted"] = written_count
        # Preserve this cycle's student as teacher for the next iteration.
        prev_student_onnx = student_onnx
        prev_student_manifest = student_manifest
        notes.append(
            f"iter{it}: self-train accepted={report.accepted} "
            f"low_conf={report.rejected_low_conf} empty={report.rejected_empty} "
            f"mid={report.mid_band} conf={conf_threshold:.4f}"
        )

        # Plateau / no-growth early stop.
        plateau, stop_reason = growth_plateau_update(
            plateau,
            accepted=written_count,
            stop_on_no_growth=bool(st_cfg.stop_on_no_growth),
            max_plateau_iters=int(st_cfg.max_plateau_iters),
        )
        if written_count <= 0 and st_cfg.stop_on_no_growth:
            if do_retrain:
                cycle["retrain_status"] = "skipped_no_labels"
                last_retrain_status = "skipped_no_labels"
                notes.append(
                    f"iter{it}: retrain skipped: no accepted labels "
                    f"(plateau={plateau}/{st_cfg.max_plateau_iters})"
                )
            else:
                cycle["retrain_status"] = "disabled"
                last_retrain_status = "disabled"
            history.append(cycle)
            if stop_reason is not None:
                stopped_reason = stop_reason
                notes.append(
                    f"self_train early stop: {stopped_reason} (consecutive zero-accept={plateau})"
                )
                break
            # Multi-iter: skip retrain, continue to next cycle (or break if last).
            continue

        # Retrain decision.
        should_retrain = do_retrain and written_count > 0
        if not do_retrain:
            cycle["retrain_status"] = "disabled"
            last_retrain_status = "disabled"
            history.append(cycle)
            continue

        if not should_retrain:
            cycle["retrain_status"] = "skipped_no_labels"
            last_retrain_status = "skipped_no_labels"
            history.append(cycle)
            continue

        try:
            retrain_updates = _run_retrain(
                config,
                paths,
                working_state,
                train_local_fn=train_local_fn,
                train_cloud_fn=train_cloud_fn,
            )
            retrain_soft = retrain_updates.pop("soft_notes", None)
            # Merge train/export artifacts into working state + final updates.
            for key, value in retrain_updates.items():
                if key == "soft_notes":
                    continue
                working_state.artifacts[key] = value
                updates[key] = value
            cycle["retrain_status"] = "ok"
            last_retrain_status = "ok"
            onnx_new = retrain_updates.get("onnx_model")
            manifest_new = retrain_updates.get("manifest")
            if onnx_new:
                working_onnx = onnx_new
                updates["retrain_onnx"] = onnx_new
            if manifest_new:
                working_manifest = manifest_new
                updates["retrain_manifest"] = manifest_new
            notes.append(f"iter{it}: retrain completed (accepted={written_count})")
            apply_merged_soft_notes(updates, retrain_soft)
            # Optional progress refresh after retrain.
            try:
                from cs2_vision_access.workflows.training.auto.stage_train import (
                    _write_train_progress,
                )

                progress_bits = _write_train_progress(config, paths, root)
                for key, value in progress_bits.items():
                    if key == "soft_notes":
                        continue
                    updates[key] = value
                    working_state.artifacts[key] = value
                apply_merged_soft_notes(updates, progress_bits)
            except Exception as progress_err:  # noqa: BLE001
                notes.append(f"iter{it}: progress refresh skipped: {progress_err}")
        except Exception as error:  # noqa: BLE001 — soft unless required
            msg = f"self_train retrain failed (iter {it}): {error}"
            cycle["retrain_status"] = "failed"
            cycle["retrain_error"] = str(error)
            last_retrain_status = "failed"
            updates["retrain_error"] = str(error)
            notes.append(msg)
            history.append(cycle)
            if st_cfg.retrain_required:
                updates["self_train_status"] = "ok"
                updates["self_train_history"] = history
                updates["self_train_notes"] = notes
                updates["self_train_iterations_completed"] = it
                updates["self_train_stopped_reason"] = "error"
                updates["self_train_labels_written"] = total_accepted
                updates["self_train_accepted"] = total_accepted
                updates["retrain_status"] = "failed"
                raise AutoTrainStageError(msg) from error
            append_soft_note(updates, msg)
            # Soft retrain failure: stop multi-iter (cannot improve student).
            stopped_reason = "error"
            break

        history.append(cycle)

    # Rank uncertain queue into progress/ when available.
    if uncertain_path is not None:
        progress_dir = paths.run_dir / "progress"
        ranked_out = rank_uncertain_queue_soft(uncertain_path, progress_dir)
        if ranked_out is not None:
            notes.append(f"ranked uncertain_queue -> {progress_dir.name}/uncertain_review.json")

    updates["self_train_status"] = "ok"
    if last_report_path is not None:
        updates["self_train_report"] = str(last_report_path.resolve())
    updates["self_train_labels_written"] = total_accepted
    updates["self_train_accepted"] = total_accepted
    updates["self_train_history"] = history
    updates["self_train_iterations_completed"] = len(history)
    updates["self_train_stopped_reason"] = stopped_reason
    updates["retrain_status"] = last_retrain_status
    if uncertain_path is not None:
        updates["uncertain_queue_path"] = str(uncertain_path.resolve())
    # Ensure working onnx/manifest (post-retrain) are on final artifacts.
    if working_onnx is not None:
        updates["onnx_model"] = working_onnx
    if working_manifest is not None:
        updates["manifest"] = working_manifest
    updates["self_train_notes"] = notes
    return updates


__all__ = ["stage_self_train"]
