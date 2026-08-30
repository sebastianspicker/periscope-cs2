"""Resume-aware multi-iter train → self-train loop with best-package tracking."""

from __future__ import annotations

import contextlib
import shutil
from collections.abc import Mapping
from dataclasses import asdict
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training import remote_autonomous_bindings as deps
from cs2_vision_access.workflows.training.contracts import Layout

from .fsutil import _extract_map_metric, _is_oom_error, _labeled_count
from .models import (
    _STATE_FILENAME,
    IterationReport,
    _iteration_report_from_dict,
    _TrainLoopResult,
)
from .state import (
    _cache_best_pt,
    _load_autonomous_state,
    _rank_and_write_uncertain_review,
    _resolve_self_train_teacher,
    _save_autonomous_state,
    _snapshot_manifest,
    _snapshot_onnx,
)


def _phase_train_self_train_loop(
    data_dir: Path,
    *,
    images_dir: Path,
    train_images_dir: Path,
    train_labels_dir: Path,
    layout: Layout,
    class_map: Mapping[int, str],
    iterations: int,
    epochs: int,
    batch_size: int,
    imgsz_resolved: int,
    base_model_resolved: str,
    lr0_resolved: float,
    patience_resolved: int,
    device: str,
    origin: str,
    conf_low: float | None,
    conf_high: float | None,
    conf_schedule: bool,
    conf_base: float,
    use_teacher_gate: bool,
    teacher_min_iou: float,
    continue_on_self_train_error: bool,
    resume: bool,
    stop_on_plateau_iters: int,
    notes: list[str],
    status: str,
) -> _TrainLoopResult:
    """Resume-aware multi-iter train → self-train loop with best-package tracking."""
    state_path = data_dir / _STATE_FILENAME
    start_iter = 1
    best_iteration: int | None = None
    best_map: float | None = None
    best_onnx_path: Path | None = None
    best_manifest_path: Path | None = None
    iteration_reports: list[IterationReport] = []
    progress_iterations: list[dict[str, Any]] = []
    labels_at_loop_start = _labeled_count(train_labels_dir)
    self_train_hard_errors: list[str] = []
    plateau_streak = 0

    if resume:
        prior = _load_autonomous_state(state_path)
        if prior is not None:
            completed = int(prior.get("completed_iters", 0) or 0)
            if completed > 0:
                start_iter = completed + 1
                notes.append(f"resume: skipping completed iters 1..{completed}")
            if prior.get("best_iteration") is not None:
                best_iteration = int(prior["best_iteration"])
            if prior.get("best_map") is not None:
                best_map = float(prior["best_map"])
            if prior.get("best_onnx_path"):
                p = Path(str(prior["best_onnx_path"]))
                if p.is_file():
                    best_onnx_path = p
            if prior.get("best_manifest_path"):
                p = Path(str(prior["best_manifest_path"]))
                if p.is_file():
                    best_manifest_path = p
            if prior.get("batch") is not None:
                with contextlib.suppress(TypeError, ValueError):
                    batch_size = max(1, int(prior["batch"]))
            prior_notes = prior.get("notes")
            if isinstance(prior_notes, list):
                for n in prior_notes:
                    if n not in notes:
                        notes.append(str(n))
            prior_reports = prior.get("iteration_reports")
            if isinstance(prior_reports, list):
                restored: list[IterationReport] = []
                for item in prior_reports:
                    if isinstance(item, dict):
                        rep = _iteration_report_from_dict(item)
                        if rep is not None:
                            restored.append(rep)
                if restored:
                    iteration_reports = restored
            prior_progress = prior.get("progress_iterations")
            if isinstance(prior_progress, list):
                progress_iterations = [
                    dict(item) for item in prior_progress if isinstance(item, dict)
                ]

    progress_dir = data_dir / "progress"
    progress_report_md: Path | None = None
    onnx_path = data_dir / deps.OUTPUT_ONNX_NAME
    manifest_path = data_dir / deps.OUTPUT_MANIFEST_NAME
    runs_project = data_dir / "runs"

    for it in range(start_iter, iterations + 1):
        print(f"\n{'=' * 60}\nShape training iteration {it}/{iterations}\n{'=' * 60}")
        labeled_before = _labeled_count(train_labels_dir)
        run_name = f"iter_{it}"
        # Snapshot prior best before this iter may promote a new best — used as
        # teacher for self-train (must not be the current student weights).
        prior_best_onnx = best_onnx_path
        prior_best_manifest = best_manifest_path

        # Each iter starts from base_model first time, then from previous best.pt
        # when available (stronger shape prior for CS2 players).
        weights = base_model_resolved
        prev_best = data_dir / "last_best.pt"
        if it > 1 and prev_best.is_file():
            weights = str(prev_best)
            notes.append(f"iter {it}: resume weights from {prev_best.name}")

        deps.write_dataset_yaml(
            data_dir,
            layout=layout,
            classes=class_map,
            portable_path=False,
        )

        # Train with OOM batch-halving retries (also updates batch for later iters).
        train_ok = False
        oom_retries = 0
        max_oom_retries = 2
        while not train_ok:
            try:
                onnx_path = deps.train(
                    data_dir,
                    base_model=weights,
                    epochs=epochs,
                    batch=batch_size,
                    imgsz=imgsz_resolved,
                    lr0=lr0_resolved,
                    patience=min(patience_resolved, max(5, epochs // 2)),
                    device=device,
                    classes=class_map,
                    resume=False,
                    verbose=True,
                    project=runs_project,
                    run_name=run_name,
                    plots=True,
                )
                train_ok = True
            except Exception as exc:  # noqa: BLE001
                if _is_oom_error(exc) and batch_size > 1 and oom_retries < max_oom_retries:
                    new_batch = max(1, batch_size // 2)
                    notes.append(f"iter {it}: OOM at batch={batch_size}; retry batch={new_batch}")
                    print(f"  CUDA OOM at batch={batch_size}; retrying with batch={new_batch}")
                    batch_size = new_batch
                    oom_retries += 1
                    continue
                raise

        # Keep Ultralytics best.pt for next iteration if we can find it.
        _cache_best_pt(data_dir, prev_best)

        # Snapshot ONNX before next iter overwrites the output name.
        iter_onnx = _snapshot_onnx(Path(onnx_path), data_dir, it)
        onnx_path = Path(onnx_path)

        manifest_path = deps.create_manifest(
            onnx_path,
            data_dir,
            classes=class_map,
            origin=f"{origin} (iter {it}/{iterations})",
        )
        # Snapshot manifest so best-iter package integrity survives later iters.
        iter_manifest = _snapshot_manifest(Path(manifest_path), data_dir, it)

        # Parse val metrics; track best package.
        run_dir = deps.find_ultralytics_run_dir(runs_project, name=run_name)
        if run_dir is None:
            run_dir = deps.find_ultralytics_run_dir(runs_project)
        metrics: dict[str, float] = {}
        if run_dir is not None:
            results_csv = run_dir / "results.csv"
            if results_csv.is_file():
                metrics = deps.parse_ultralytics_results_csv(results_csv)
        map_primary, map50_95 = _extract_map_metric(metrics)
        map50 = float(metrics["mAP50"]) if "mAP50" in metrics else None

        improved = False
        if map_primary is not None and (best_map is None or map_primary > best_map):
            best_map = map_primary
            best_iteration = it
            best_onnx_path = iter_onnx if iter_onnx.is_file() else onnx_path
            best_manifest_path = iter_manifest if iter_manifest.is_file() else Path(manifest_path)
            improved = True
            # Keep canonical download names pointing at current best.
            try:
                if best_onnx_path.is_file():
                    shutil.copy2(best_onnx_path, data_dir / deps.OUTPUT_ONNX_NAME)
                if best_manifest_path.is_file():
                    shutil.copy2(best_manifest_path, data_dir / deps.OUTPUT_MANIFEST_NAME)
            except OSError as exc:
                notes.append(f"iter {it}: could not copy best outputs: {exc}")
        elif map_primary is not None:
            plateau_streak += 1
        if improved:
            plateau_streak = 0

        # Self-train: grow silhouette coverage on train split only.
        # Optionally raise conf over iterations (stricter pseudo-labels later).
        conf_it, conf_low_resolved, conf_high_resolved = deps.resolve_conf_bands(
            conf_base,
            conf_low=conf_low,
            conf_high=conf_high,
            iteration=it,
            conf_schedule=bool(conf_schedule),
        )
        # Multi-iter teacher gate: previous best / last-iter ONNX (never iter 1).
        teacher_model, teacher_manifest = _resolve_self_train_teacher(
            data_dir,
            it,
            prior_best_onnx=prior_best_onnx,
            prior_best_manifest=prior_best_manifest,
            use_teacher_gate=use_teacher_gate,
        )
        teacher_note = ""
        if teacher_model is not None:
            teacher_note = f", teacher={Path(teacher_model).name}"
            notes.append(
                f"iter {it}: self-train teacher={Path(teacher_model).name} "
                f"min_iou={teacher_min_iou}"
            )
        elif use_teacher_gate and it > 1:
            notes.append(
                f"iter {it}: teacher gate enabled but no prior ONNX/manifest; "
                "self-train without teacher"
            )
        schedule_note = ", scheduled" if conf_schedule else ""
        print(
            f"Shape self-train: high-conf player silhouette re-label "
            f"(conf={conf_it:.3f}, conf_high={conf_high_resolved:.3f}, "
            f"conf_low={conf_low_resolved:.3f}{schedule_note}"
            f"{teacher_note})..."
        )
        accepted = 0
        rejected = 0
        try:
            st_kwargs: dict[str, Any] = dict(
                conf_threshold=conf_it,
                conf_low=conf_low_resolved,
                conf_high=conf_high_resolved,
                uncertain_queue_path=data_dir / "uncertain_queue.jsonl",
                device=device,
                write_policy="overwrite_pseudo",
                allowed_class_ids=set(class_map.keys()),
                fail_closed=True,
                report_path=data_dir / f"self_train_iter_{it}.json",
            )
            if teacher_model is not None and teacher_manifest is not None:
                st_kwargs["teacher_model"] = teacher_model
                st_kwargs["teacher_manifest"] = teacher_manifest
                st_kwargs["teacher_min_iou"] = float(teacher_min_iou)
            st = deps.run_self_train_iteration(
                train_images_dir,
                train_labels_dir,
                onnx_path,
                manifest_path,
                **st_kwargs,
            )
            accepted = st.accepted
            rejected = st.rejected_low_conf
            notes.append(
                f"iter {it}: self-train accepted={accepted} "
                f"low_conf={rejected} empty={st.rejected_empty} mid={st.mid_band} "
                f"conf={conf_it:.3f}"
            )
        except Exception as exc:  # noqa: BLE001
            msg = f"iter {it}: self-train error: {exc}"
            print(f"(self-train error: {exc})")
            notes.append(msg)
            self_train_hard_errors.append(msg)
            if not continue_on_self_train_error:
                status = "failed"
                # Persist state then re-raise so mid-loop resume can continue later.
                _save_autonomous_state(
                    state_path,
                    {
                        "completed_iters": it - 1,
                        "best_iteration": best_iteration,
                        "best_map": best_map,
                        "best_onnx_path": str(best_onnx_path) if best_onnx_path else None,
                        "best_manifest_path": (
                            str(best_manifest_path) if best_manifest_path else None
                        ),
                        "batch": batch_size,
                        "notes": notes,
                        "status": status,
                        "iteration_reports": [asdict(r) for r in iteration_reports],
                        "progress_iterations": list(progress_iterations),
                    },
                )
                raise RuntimeError(msg) from exc

        labeled_after = _labeled_count(train_labels_dir)
        iteration_reports.append(
            IterationReport(
                iteration=it,
                onnx=str(onnx_path),
                manifest=str(manifest_path),
                labeled_before=labeled_before,
                labeled_after=labeled_after,
                self_train_accepted=accepted,
                self_train_rejected_low_conf=rejected,
                map50=map50,
                map50_95=map50_95,
            )
        )

        # Progress report: locate Ultralytics run artifacts for this iteration
        iter_progress: dict[str, Any] = {
            "iteration": it,
            "labeled_before": labeled_before,
            "labeled_after": labeled_after,
            "self_train_accepted": accepted,
            "self_train_rejected_low_conf": rejected,
            "map50": map50,
            "map50_95": map50_95,
        }
        if run_dir is not None:
            results_csv = run_dir / "results.csv"
            if results_csv.is_file():
                iter_progress["results_csv"] = str(results_csv)
            results_png = run_dir / "results.png"
            if results_png.is_file():
                iter_progress["results_png"] = str(results_png)
        progress_iterations.append(iter_progress)

        try:
            progress_report_md = deps.write_progress_report(
                progress_dir,
                labels_dir=train_labels_dir,
                class_names=class_map,
                iterations=progress_iterations,
                title="Autonomous player-shape training progress",
                notes=list(notes),
                images_count=deps.count_images(train_images_dir)
                if train_images_dir != images_dir
                else deps.count_images(images_dir),
            )
            print(f"  Progress report: {progress_report_md}")
        except Exception as exc:  # noqa: BLE001 — never break the loop on plots
            print(f"  (progress report skipped: {exc})")
            notes.append(f"iter {it}: progress report error: {exc}")

        # Rank self-train uncertain queue for human review (soft; under progress/).
        try:
            _rank_and_write_uncertain_review(data_dir, progress_dir, notes=notes)
        except Exception as exc:  # noqa: BLE001
            print(f"  (uncertain rank skipped: {exc})")
            notes.append(f"iter {it}: uncertain rank error: {exc}")

        # Persist mid-loop state for resume.
        _save_autonomous_state(
            state_path,
            {
                "completed_iters": it,
                "best_iteration": best_iteration,
                "best_map": best_map,
                "best_onnx_path": str(best_onnx_path) if best_onnx_path else None,
                "best_manifest_path": (str(best_manifest_path) if best_manifest_path else None),
                "batch": batch_size,
                "notes": notes,
                "status": status,
                "iteration_reports": [asdict(r) for r in iteration_reports],
                "progress_iterations": list(progress_iterations),
            },
        )

        print(
            f"Shape self-train iter {it} done: labels "
            f"{labeled_before} → {labeled_after} "
            f"(+{labeled_after - labeled_before})"
            + (f" mAP={map_primary:.4f}" if map_primary is not None else "")
        )

        # Plateau early-stop (P1-10).
        if (
            stop_on_plateau_iters > 0
            and plateau_streak >= stop_on_plateau_iters
            and it < iterations
        ):
            notes.append(
                f"early stop: mAP plateau for {plateau_streak} iters "
                f"(stop_on_plateau_iters={stop_on_plateau_iters})"
            )
            print(f"  Plateau early-stop after iteration {it}")
            break

    return _TrainLoopResult(
        status=status,
        batch_size=batch_size,
        onnx_path=Path(onnx_path),
        manifest_path=Path(manifest_path),
        iteration_reports=iteration_reports,
        best_iteration=best_iteration,
        best_map=best_map,
        best_onnx_path=best_onnx_path,
        best_manifest_path=best_manifest_path,
        progress_dir=progress_dir,
        progress_report_md=progress_report_md,
        labels_at_loop_start=labels_at_loop_start,
        self_train_hard_errors=self_train_hard_errors,
    )
