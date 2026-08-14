"""Autonomous loop state, snapshots, teacher resolution, and progress helpers."""

from __future__ import annotations

import json
import shutil
import zipfile
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from . import deps


def _append_progress_to_bundle(bundle: Path, progress_dir: Path) -> None:
    """Append ``progress/`` tree into an existing bundle zip (best-effort)."""
    if not bundle.is_file() or not progress_dir.is_dir():
        return
    try:
        with zipfile.ZipFile(bundle, "a", compression=zipfile.ZIP_DEFLATED) as zf:
            existing = set(zf.namelist())
            for path in sorted(progress_dir.rglob("*")):
                if not path.is_file():
                    continue
                rel = path.relative_to(progress_dir).as_posix()
                arcname = f"progress/{rel}"
                if arcname in existing:
                    continue
                zf.write(path, arcname=arcname)
        print(f"  Appended progress/ to {bundle.name}")
    except OSError as exc:
        print(f"  (could not append progress/ to bundle: {exc})")


def _rank_and_write_uncertain_review(
    data_dir: Path,
    progress_dir: Path,
    *,
    top_k: int = 50,
    notes: list[str] | None = None,
) -> int | None:
    """Rank ``uncertain_queue.jsonl`` into progress/ (best-effort, never raises).

    Returns number of ranked frames, or None when queue missing/empty/skipped.
    Thin wrapper over :func:`rank_uncertain_queue_soft` that reports count.
    """
    queue_path = Path(data_dir) / "uncertain_queue.jsonl"
    out = deps.rank_uncertain_queue_soft(queue_path, progress_dir, top_k=top_k)
    if out is None:
        return None
    try:
        payload = json.loads(out.read_text(encoding="utf-8"))
        n = len(payload) if isinstance(payload, list) else 0
        print(f"  Uncertain review queue: {n} frame(s) -> {Path(progress_dir).name}/")
        return n
    except Exception as exc:  # noqa: BLE001 — soft; never break autonomous loop
        msg = f"uncertain_queue rank skipped: {exc}"
        print(f"  ({msg})")
        if notes is not None:
            notes.append(msg)
        return None


def _load_autonomous_state(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return data if isinstance(data, dict) else None


def _save_autonomous_state(path: Path, state: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(dict(state), indent=2) + "\n", encoding="utf-8")


def _snapshot_onnx(onnx_path: Path, data_dir: Path, iteration: int) -> Path:
    """Copy iteration ONNX beside data_dir so later iters do not clobber best."""
    dest = data_dir / f"cs2-yolo11n-seg.iter{iteration}.onnx"
    if onnx_path.is_file():
        try:
            if onnx_path.resolve() != dest.resolve():
                shutil.copy2(onnx_path, dest)
        except OSError:
            pass
    return dest


def _snapshot_manifest(manifest_path: Path, data_dir: Path, iteration: int) -> Path:
    """Copy iteration manifest beside data_dir so later iters do not clobber best."""
    dest = data_dir / f"cs2-yolo11n-seg.iter{iteration}.model.json"
    if manifest_path.is_file():
        try:
            if manifest_path.resolve() != dest.resolve():
                shutil.copy2(manifest_path, dest)
        except OSError:
            pass
    return dest


def _resolve_self_train_teacher(
    data_dir: Path,
    iteration: int,
    *,
    prior_best_onnx: Path | None,
    prior_best_manifest: Path | None,
    use_teacher_gate: bool,
) -> tuple[Path | None, Path | None]:
    """Resolve teacher ONNX + manifest for multi-iter self-train.

    Thin wrapper around :class:`BestPackageTeacherStrategy` so tests can still
    patch ``_resolve_self_train_teacher``. Iteration 1 has no prior student
    ONNX (base is usually ``.pt``), so teacher is skipped. Later iters prefer
    the best package from prior iterations, then fall back to the previous
    iteration snapshot.
    """
    if not use_teacher_gate:
        return None, None
    pair = deps.BestPackageTeacherStrategy().resolve(
        iteration=iteration,
        best_onnx=prior_best_onnx,
        best_manifest=prior_best_manifest,
        data_dir=data_dir,
    )
    if pair is None:
        return None, None
    return pair.model, pair.manifest


def _cache_best_pt(data_dir: Path, dest: Path) -> None:
    """Best-effort copy of the latest Ultralytics best.pt beside data_dir."""
    candidates: list[Path] = []
    for root in (Path.cwd() / "runs", data_dir / "runs", Path("runs")):
        if not root.is_dir():
            continue
        candidates.extend(root.rglob("best.pt"))
    if not candidates:
        return
    newest = max(candidates, key=lambda p: p.stat().st_mtime)
    try:
        shutil.copy2(newest, dest)
        print(f"  Cached {newest} → {dest}")
    except OSError:
        pass
