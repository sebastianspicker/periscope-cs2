"""Named strategies for multi-iter self-train teacher resolution.

Two primary policies:

* ``prev_student`` / ``auto`` — local auto-train: iteration 1 uses optional
  explicit teacher paths; later iterations use the previous cycle's student
  ONNX + manifest.
* ``best_package`` / ``remote`` — remote autonomous: prefer the prior best
  package; fall back to the on-disk ``iter{N-1}`` snapshot under ``data_dir``.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


@dataclass(frozen=True)
class TeacherPair:
    """Resolved teacher ONNX model + companion manifest paths."""

    model: Path
    manifest: Path


class TeacherStrategy(Protocol):
    """Resolve which ONNX/manifest pair should gate the current self-train iter."""

    name: str

    def resolve(
        self,
        *,
        iteration: int,  # 1-based
        prev_student_onnx: Path | str | None = None,
        prev_student_manifest: Path | str | None = None,
        best_onnx: Path | str | None = None,
        best_manifest: Path | str | None = None,
        data_dir: Path | str | None = None,
        explicit_onnx: Path | str | None = None,
        explicit_manifest: Path | str | None = None,
    ) -> TeacherPair | None: ...


def _pair_if_both(
    model: Path | str | None,
    manifest: Path | str | None,
    *,
    require_files: bool = False,
) -> TeacherPair | None:
    """Return a TeacherPair when both paths are set (and optionally on disk)."""
    if model is None or manifest is None:
        return None
    model_p = Path(model)
    manifest_p = Path(manifest)
    if require_files and not (model_p.is_file() and manifest_p.is_file()):
        return None
    return TeacherPair(model=model_p, manifest=manifest_p)


class PrevStudentTeacherStrategy:
    """Auto multi-iter: iter1 optional explicit; it>1 previous student ONNX."""

    name = "prev_student"

    def resolve(
        self,
        *,
        iteration: int,
        prev_student_onnx: Path | str | None = None,
        prev_student_manifest: Path | str | None = None,
        best_onnx: Path | str | None = None,
        best_manifest: Path | str | None = None,
        data_dir: Path | str | None = None,
        explicit_onnx: Path | str | None = None,
        explicit_manifest: Path | str | None = None,
    ) -> TeacherPair | None:
        del best_onnx, best_manifest, data_dir  # unused by this strategy
        it = int(iteration)
        if it <= 1:
            return _pair_if_both(explicit_onnx, explicit_manifest)
        return _pair_if_both(prev_student_onnx, prev_student_manifest)


class BestPackageTeacherStrategy:
    """Remote multi-iter: prefer prior best package; else iter{N-1} snapshot."""

    name = "best_package"

    def resolve(
        self,
        *,
        iteration: int,
        prev_student_onnx: Path | str | None = None,
        prev_student_manifest: Path | str | None = None,
        best_onnx: Path | str | None = None,
        best_manifest: Path | str | None = None,
        data_dir: Path | str | None = None,
        explicit_onnx: Path | str | None = None,
        explicit_manifest: Path | str | None = None,
    ) -> TeacherPair | None:
        del prev_student_onnx, prev_student_manifest  # unused by this strategy
        it = int(iteration)
        if it <= 1:
            return _pair_if_both(explicit_onnx, explicit_manifest)
        best = _pair_if_both(best_onnx, best_manifest, require_files=True)
        if best is not None:
            return best
        if data_dir is None:
            return None
        root = Path(data_dir)
        prev = it - 1
        prev_onnx = root / f"cs2-yolo11n-seg.iter{prev}.onnx"
        prev_manifest = root / f"cs2-yolo11n-seg.iter{prev}.model.json"
        return _pair_if_both(prev_onnx, prev_manifest, require_files=True)


def get_teacher_strategy(name: str) -> TeacherStrategy:
    """Return a named teacher strategy instance.

    Accepted aliases:

    * ``prev_student`` / ``auto``
    * ``best_package`` / ``remote``
    """
    key = (name or "").strip().lower()
    if key in ("prev_student", "auto"):
        return PrevStudentTeacherStrategy()
    if key in ("best_package", "remote"):
        return BestPackageTeacherStrategy()
    raise ValueError(
        f"unknown teacher strategy {name!r}; expected one of: "
        "prev_student, auto, best_package, remote"
    )
