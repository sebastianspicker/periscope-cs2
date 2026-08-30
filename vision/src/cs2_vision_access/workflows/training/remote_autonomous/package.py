"""Best-package install, quality gates, report write, and output packaging."""

from __future__ import annotations

import json
import shutil
from collections.abc import Mapping
from pathlib import Path

from cs2_vision_access.workflows.training import remote_autonomous_bindings as deps

from .fsutil import _labeled_count
from .models import (
    _CS2_10K_HOLDOUT_JSON,
    AutonomousReport,
    _TrainLoopResult,
)
from .state import _append_progress_to_bundle, _rank_and_write_uncertain_review


def _align_fp16_with_best(
    onnx_path: Path,
    *,
    force: bool,
    notes: list[str],
) -> None:
    """Ensure the FP16 sibling matches *onnx_path* (best weights).

    When the last train wrote an FP16 for a non-best iteration, re-convert from
    best or remove the stale FP16 so :func:`package_outputs` cannot ship the
    wrong weights next to best FP32. When *force* is False, leave any existing
    last-iter FP16 alone (it already matches best).
    """
    if not force:
        return
    onnx_path = Path(onnx_path)
    if not onnx_path.is_file():
        return
    fp16_path = onnx_path.with_name(f"{onnx_path.stem}-fp16{onnx_path.suffix}")
    if fp16_path.is_file():
        try:
            fp16_path.unlink()
        except OSError as exc:
            notes.append(f"could not remove stale FP16 before re-convert: {exc}")
    try:
        from cs2_vision_access.application.ports.model_runtime import convert_to_fp16

        convert_to_fp16(onnx_path, fp16_path)
        print(f"  Best-iter FP16: {fp16_path.name}")
    except Exception as exc:  # noqa: BLE001 — optional artefact
        notes.append(f"best-iter FP16 convert skipped: {exc}")
        # Drop mismatched last-iter FP16 from package consideration.
        if fp16_path.is_file():
            try:
                fp16_path.unlink()
                notes.append(f"removed stale FP16 (not matching best): {fp16_path.name}")
            except OSError as unlink_exc:
                notes.append(f"could not remove stale FP16: {unlink_exc}")


def _phase_package_report(
    data_dir: Path,
    *,
    images_dir: Path,
    train_labels_dir: Path,
    class_map: Mapping[int, str],
    origin: str,
    device: str,
    bootstrap_written: int,
    min_map50: float | None,
    min_label_growth: int,
    continue_on_self_train_error: bool,
    loop: _TrainLoopResult,
    dataset_license: str | None,
    dataset_source: str | None,
    notes: list[str],
) -> AutonomousReport:
    """Install best package, apply quality gates, write report, and zip outputs."""
    status = loop.status
    onnx_path = loop.onnx_path
    manifest_path = loop.manifest_path
    best_onnx_path = loop.best_onnx_path
    best_manifest_path = loop.best_manifest_path
    best_iteration = loop.best_iteration
    best_map = loop.best_map
    iteration_reports = loop.iteration_reports
    progress_dir = loop.progress_dir
    progress_report_md = loop.progress_report_md
    labels_at_loop_start = loop.labels_at_loop_start
    self_train_hard_errors = loop.self_train_hard_errors

    # If no metric ever improved, fall back to last ONNX as "best".
    if best_onnx_path is None and onnx_path.is_file():
        best_onnx_path = Path(onnx_path)
        best_manifest_path = Path(manifest_path)
        best_iteration = iteration_reports[-1].iteration if iteration_reports else None

    # Ensure download convenience names point at best package.
    final_onnx = data_dir / deps.OUTPUT_ONNX_NAME
    final_manifest = data_dir / deps.OUTPUT_MANIFEST_NAME
    if best_onnx_path is not None and best_onnx_path.is_file():
        try:
            if best_onnx_path.resolve() != final_onnx.resolve():
                shutil.copy2(best_onnx_path, final_onnx)
            onnx_path = final_onnx
        except OSError as exc:
            notes.append(f"could not install best onnx to output name: {exc}")
            onnx_path = best_onnx_path
    if best_manifest_path is not None and best_manifest_path.is_file():
        try:
            if best_manifest_path.resolve() != final_manifest.resolve():
                shutil.copy2(best_manifest_path, final_manifest)
            manifest_path = final_manifest
        except OSError as exc:
            notes.append(f"could not install best manifest to output name: {exc}")
            # Re-create from best ONNX when the snapshot copy fails.
            try:
                manifest_path = deps.create_manifest(
                    onnx_path,
                    data_dir,
                    classes=class_map,
                    origin=f"{origin} (best iter {best_iteration})",
                )
            except Exception as create_exc:  # noqa: BLE001
                notes.append(f"could not re-create best manifest: {create_exc}")
                manifest_path = best_manifest_path
    elif Path(onnx_path).is_file() and not Path(manifest_path).is_file():
        try:
            manifest_path = deps.create_manifest(
                onnx_path,
                data_dir,
                classes=class_map,
                origin=f"{origin} (best iter {best_iteration})",
            )
        except Exception as create_exc:  # noqa: BLE001
            notes.append(f"could not create best manifest: {create_exc}")

    # FP16 must match best FP32 (last train may have written a worse-iter FP16).
    last_completed = iteration_reports[-1].iteration if iteration_reports else None
    force_fp16 = (
        best_iteration is not None
        and last_completed is not None
        and best_iteration != last_completed
    )
    _align_fp16_with_best(Path(onnx_path), force=force_fp16, notes=notes)

    # Quality gates (P1-10).
    if min_map50 is not None and (best_map is None or best_map < float(min_map50)):
        status = "degraded"
        notes.append(f"quality gate: best_map={best_map} < min_map50={min_map50}")
    labels_final = _labeled_count(train_labels_dir)
    growth = labels_final - labels_at_loop_start
    if min_label_growth > 0 and growth < int(min_label_growth):
        status = "degraded"
        notes.append(f"quality gate: label growth {growth} < min_label_growth={min_label_growth}")
    if self_train_hard_errors and not continue_on_self_train_error:
        status = "failed"
        raise RuntimeError("self-train hard error(s): " + "; ".join(self_train_hard_errors))

    progress_dir_str: str | None = None
    progress_md_str: str | None = None
    if progress_dir is not None and progress_dir.is_dir() and any(progress_dir.iterdir()):
        progress_dir_str = str(progress_dir.resolve())
    if progress_report_md is not None and Path(progress_report_md).is_file():
        progress_md_str = str(Path(progress_report_md).resolve())
    elif progress_dir is not None and (progress_dir / "report.md").is_file():
        progress_md_str = str((progress_dir / "report.md").resolve())

    # Final uncertain-queue ranking into progress/ (packaged with report).
    if progress_dir is not None:
        try:
            _rank_and_write_uncertain_review(data_dir, progress_dir, notes=notes)
        except Exception as exc:  # noqa: BLE001
            print(f"  (final uncertain rank skipped: {exc})")
            notes.append(f"final uncertain rank error: {exc}")

    # Write report before packaging so it can be included via extra_paths.
    report_path = data_dir / "autonomous_report.json"
    bundle_path: str | None = None
    report = AutonomousReport(
        data_dir=str(data_dir.resolve()),
        onnx_path=str(Path(onnx_path).resolve()),
        manifest_path=str(Path(manifest_path).resolve()),
        bundle_path=bundle_path,
        iterations=iteration_reports,
        bootstrap_labels_written=bootstrap_written,
        final_label_count=labels_final,
        final_image_count=deps.count_images(images_dir),
        device=device,
        origin=origin,
        notes=notes,
        progress_dir=progress_dir_str,
        progress_report_md=progress_md_str,
        best_iteration=best_iteration,
        best_map=best_map,
        best_onnx_path=(
            str(best_onnx_path.resolve())
            if best_onnx_path is not None and Path(best_onnx_path).is_file()
            else (str(Path(onnx_path).resolve()) if Path(onnx_path).is_file() else None)
        ),
        status=status,
        dataset_license=dataset_license,
        dataset_source=dataset_source,
    )
    report_path.write_text(json.dumps(report.as_dict(), indent=2) + "\n", encoding="utf-8")

    # --- Package (best, with provenance) ------------------------------------
    if Path(onnx_path).is_file() and Path(manifest_path).is_file():
        try:
            extra_dirs: list[tuple[str | Path, str]] = []
            if progress_dir is not None and progress_dir.is_dir() and any(progress_dir.iterdir()):
                extra_dirs.append((progress_dir, "progress"))
            extra_paths: list[str | Path] = []
            held_out_plan = data_dir / "held_out_split.json"
            if held_out_plan.is_file():
                extra_paths.append(held_out_plan)
            holdout_json = data_dir / _CS2_10K_HOLDOUT_JSON
            if holdout_json.is_file():
                extra_paths.append(holdout_json)
            if report_path.is_file():
                extra_paths.append(report_path)
            bundle = deps.package_outputs(
                onnx_path,
                manifest_path,
                data_dir / "cs2-yolo11n-seg-bundle.zip",
                extra_dirs=extra_dirs or None,
                extra_paths=extra_paths or None,
            )
            bundle_path = str(bundle)
            if not Path(bundle).is_file():
                status = "degraded"
                notes.append("package_outputs returned missing bundle")
            elif progress_dir is not None and progress_dir.is_dir() and any(progress_dir.iterdir()):
                _append_progress_to_bundle(Path(bundle), progress_dir)
        except Exception as exc:  # noqa: BLE001
            notes.append(f"package_outputs: {exc}")
            status = "degraded"
            bundle_path = None
    elif Path(onnx_path).is_file():
        # ONNX exists but packaging could not run (e.g. missing manifest).
        status = "degraded"
        notes.append("package skipped: onnx present but manifest missing or unusable")

    # Refresh report with final bundle_path / status / notes.
    report.bundle_path = bundle_path
    report.status = status
    report.notes = notes
    report_path.write_text(json.dumps(report.as_dict(), indent=2) + "\n", encoding="utf-8")

    print(f"\n✓ Player-shape autonomous report: {report_path}")
    print(f"  Status:         {report.status}")
    print(f"  Best iter/mAP:  {report.best_iteration} / {report.best_map}")
    print(f"  Final ONNX:     {report.onnx_path}")
    print(f"  Final labels:   {report.final_label_count}/{report.final_image_count}")
    if dataset_license:
        print(f"  Dataset:        {dataset_source} ({dataset_license})")
    if progress_md_str:
        print(f"  Progress MD:    {progress_md_str}")
    if bundle_path:
        print(f"  Download zip:   {bundle_path}")

    if status == "failed":
        raise RuntimeError("autonomous loop finished with status=failed; see notes")
    return report
