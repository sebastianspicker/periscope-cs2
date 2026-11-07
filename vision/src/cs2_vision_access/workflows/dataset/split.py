"""Assemble YOLO train/val/test trees from staged session directories.

Operators annotate per-session staging folders, then assign whole ``session_id``
values to train, val, or test. Adjacent frames within a session are never
randomly split across partitions, that would measure memorization, not
generalization.

Plan types and builders live in :mod:`cs2_vision_access.workflows.dataset.split_plan`;
this module re-exports them for stable import paths.
"""

from __future__ import annotations

import json
import os
import shutil
import tempfile
from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.workflows.dataset.split_plan import (
    DatasetSplitError,
    SessionSplitPlan,
    build_split_plan,
    load_split_plan,
)
from cs2_vision_access.workflows.dataset.types import (
    DEFAULT_SESSIONS_FILENAME,
    IMAGE_EXTENSIONS,
    OPTIONAL_SPLITS,
    REQUIRED_SPLITS,
)

__all__ = [
    "AssembleDatasetSummary",
    "DatasetSplitError",
    "SessionSplitPlan",
    "assemble_dataset",
    "build_split_plan",
    "discover_session_directories",
    "load_split_plan",
]


@dataclass(frozen=True)
class AssembleDatasetSummary:
    staging_root: str
    output_root: str
    train_sessions: tuple[str, ...]
    val_sessions: tuple[str, ...]
    test_sessions: tuple[str, ...]
    image_count: int
    label_count: int
    sessions_path: str
    copied_files: int

    def as_dict(self) -> dict[str, object]:
        return {
            "staging_root": self.staging_root,
            "output_root": self.output_root,
            "train_sessions": list(self.train_sessions),
            "val_sessions": list(self.val_sessions),
            "test_sessions": list(self.test_sessions),
            "image_count": self.image_count,
            "label_count": self.label_count,
            "sessions_path": self.sessions_path,
            "copied_files": self.copied_files,
        }


def discover_session_directories(staging_root: str | Path) -> dict[str, Path]:
    """Map session_id → staging directory under the staging root."""
    root = Path(staging_root)
    if root.is_symlink():
        raise DatasetSplitError("staging root must not be a symlink")
    if not root.is_dir():
        raise DatasetSplitError(f"staging root is not a directory: {root}")
    discovered: dict[str, Path] = {}
    for child in sorted(root.iterdir(), key=lambda path: path.name):
        if child.name.startswith("."):
            continue
        if child.is_symlink():
            raise DatasetSplitError(f"session directory must not be a symlink: {child.name}")
        if not child.is_dir():
            continue
        discovered[child.name] = child
    return discovered


def assemble_dataset(
    staging_root: str | Path,
    output_root: str | Path,
    plan: SessionSplitPlan,
    *,
    overwrite: bool = False,
) -> AssembleDatasetSummary:
    """Copy staged sessions into a YOLO images/labels layout by whole session_id.

    Destination files live under ``images/{split}/{session_id}/`` and
    ``labels/{split}/{session_id}/`` so audit directory-prefix mapping stays
    trivial. A ``sessions.json`` map of ``session_id`` → ``session_id`` is
    written next to the tree for ``audit-dataset``.

    ``plan`` is re-validated via :func:`build_split_plan` so hand-built
    ``SessionSplitPlan`` values with empty train/val, duplicate ids, or
    multi-split ownership fail closed before any files are written.
    """
    # Re-normalize even for caller-built plans; do not trust dataclass construction.
    plan = build_split_plan(train=plan.train, val=plan.val, test=plan.test)

    staging = Path(staging_root)
    output = Path(output_root)
    if staging.is_symlink():
        raise DatasetSplitError("staging root must not be a symlink")
    if not staging.is_dir():
        raise DatasetSplitError(f"staging root is not a directory: {staging}")
    if output.is_symlink():
        raise DatasetSplitError("output root must not be a symlink")

    sessions = discover_session_directories(staging)
    missing = [session_id for session_id in plan.all_sessions() if session_id not in sessions]
    if missing:
        raise DatasetSplitError(
            "missing staging session directories: " + ", ".join(sorted(missing))
        )

    unused = sorted(set(sessions) - set(plan.all_sessions()))
    if unused:
        raise DatasetSplitError(
            "staging contains unassigned sessions (omit them from staging or "
            "assign them): " + ", ".join(unused)
        )

    _check_output_root(output, overwrite=overwrite)

    # Validate every session before touching the output root.
    collected: list[tuple[str, str, list[tuple[Path, Path, str]]]] = []
    for split in REQUIRED_SPLITS + OPTIONAL_SPLITS:
        for session_id in plan.sessions_for(split):
            collected.append(
                (split, session_id, _collect_session_pairs(sessions[session_id], session_id))
            )

    output.mkdir(parents=True, exist_ok=True)
    # Build beside the output so the final swap is a same-filesystem rename.
    work = Path(tempfile.mkdtemp(dir=output.parent, prefix=f".{output.name}.assemble-"))
    build = work / "new"
    build.mkdir()
    image_count = 0
    label_count = 0
    copied_files = 0
    try:
        for split, session_id, pairs in collected:
            for image_path, label_path, relative_stem in pairs:
                relative = Path(relative_stem)
                destination_image = (
                    build
                    / "images"
                    / split
                    / session_id
                    / relative.with_suffix(image_path.suffix.lower())
                )
                destination_label = (
                    build / "labels" / split / session_id / relative.with_suffix(".txt")
                )
                destination_image.parent.mkdir(parents=True, exist_ok=True)
                destination_label.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(image_path, destination_image)
                shutil.copy2(label_path, destination_label)
                image_count += 1
                label_count += 1
                copied_files += 2
        _write_sessions_json(build, plan)
        _swap_into_place(build, output, work / "old")
    except _RollbackFailedError:
        raise  # work/ still holds the previous dataset; keep it for manual recovery
    except BaseException:
        _remove_work_dir(work, output)
        raise
    _remove_work_dir(work, output)

    sessions_path = (output / DEFAULT_SESSIONS_FILENAME).resolve()
    return AssembleDatasetSummary(
        staging_root=str(staging.resolve()),
        output_root=str(output.resolve()),
        train_sessions=plan.train,
        val_sessions=plan.val,
        test_sessions=plan.test,
        image_count=image_count,
        label_count=label_count,
        sessions_path=str(sessions_path),
        copied_files=copied_files,
    )


class _RollbackFailedError(DatasetSplitError):
    """Swap failed and the previous dataset could not be restored."""


_MANAGED_NAMES = ("images", "labels", DEFAULT_SESSIONS_FILENAME)


def _check_output_root(output: Path, *, overwrite: bool) -> None:
    """Reject a bad output root without modifying it."""
    if not output.exists():
        return
    if not output.is_dir():
        raise DatasetSplitError(f"output root is not a directory: {output}")
    existing = [
        name for name in _MANAGED_NAMES if (output / name).exists() or (output / name).is_symlink()
    ]
    if existing and not overwrite:
        raise DatasetSplitError(
            "output root already has images/, labels/, or sessions.json; "
            "pass overwrite=True / --overwrite to replace the assembled dataset"
        )


def _swap_into_place(build: Path, output: Path, old: Path) -> None:
    """Move the old managed entries aside, move the new ones in; roll back on failure."""
    old.mkdir()
    moved_old: list[str] = []
    moved_new: list[str] = []
    try:
        for name in _MANAGED_NAMES:
            target = output / name
            if target.exists() or target.is_symlink():
                os.replace(target, old / name)
                moved_old.append(name)
            os.replace(build / name, target)
            moved_new.append(name)
    except BaseException as error:  # incl. KeyboardInterrupt: never leave output half-swapped
        try:
            for name in moved_new:
                os.replace(output / name, build / name)
            for name in moved_old:
                os.replace(old / name, output / name)
        except OSError as rollback_error:
            raise _RollbackFailedError(
                f"could not restore previous dataset ({rollback_error}); it is kept in {old}"
            ) from error
        if isinstance(error, OSError):
            raise DatasetSplitError(f"could not replace assembled dataset: {error}") from error
        raise


def _remove_work_dir(work: Path, output: Path) -> None:
    """Remove the temp dir this call created; refuse anything that is not one."""
    expected_parent = output.parent.resolve()
    if work.resolve().parent != expected_parent or not work.name.startswith(
        f".{output.name}.assemble-"
    ):
        raise DatasetSplitError(f"refusing to remove unexpected work directory: {work}")
    shutil.rmtree(work)


def _collect_session_pairs(session_dir: Path, session_id: str) -> list[tuple[Path, Path, str]]:
    images_dir = session_dir / "images"
    labels_dir = session_dir / "labels"
    if images_dir.is_dir() and not images_dir.is_symlink():
        if labels_dir.is_symlink() or not labels_dir.is_dir():
            raise DatasetSplitError(f"session {session_id!r} has images/ but no labels/ directory")
        return _pairs_from_trees(images_dir, labels_dir, session_id)

    image_files = _files_under(session_dir, IMAGE_EXTENSIONS)
    # Ignore nested YOLO split trees accidentally staged flat.
    image_files = [
        path for path in image_files if "images" not in path.relative_to(session_dir).parts[:1]
    ]
    if not image_files:
        raise DatasetSplitError(f"session {session_id!r} contains no supported images")

    pairs: list[tuple[Path, Path, str]] = []
    for image_path in image_files:
        relative = image_path.relative_to(session_dir)
        relative_stem = relative.with_suffix("").as_posix()
        label_path = image_path.with_suffix(".txt")
        if label_path.is_symlink():
            raise DatasetSplitError(
                f"session {session_id!r} label must not be a symlink: {relative_stem}"
            )
        if not label_path.is_file():
            raise DatasetSplitError(
                f"session {session_id!r} image missing same-stem label: {relative.as_posix()}"
            )
        pairs.append((image_path, label_path, relative_stem))
    _reject_orphan_flat_labels(session_dir, image_files, session_id)
    return pairs


def _pairs_from_trees(
    images_dir: Path, labels_dir: Path, session_id: str
) -> list[tuple[Path, Path, str]]:
    image_files = _files_under(images_dir, IMAGE_EXTENSIONS)
    if not image_files:
        raise DatasetSplitError(f"session {session_id!r} images/ contains no supported images")
    label_files = _files_under(labels_dir, frozenset({".txt"}))
    labels_by_stem = {
        path.relative_to(labels_dir).with_suffix("").as_posix(): path for path in label_files
    }
    pairs: list[tuple[Path, Path, str]] = []
    seen_stems: set[str] = set()
    for image_path in image_files:
        relative_stem = image_path.relative_to(images_dir).with_suffix("").as_posix()
        label_path = labels_by_stem.get(relative_stem)
        if label_path is None:
            raise DatasetSplitError(f"session {session_id!r} image missing label: {relative_stem}")
        pairs.append((image_path, label_path, relative_stem))
        seen_stems.add(relative_stem)
    orphans = sorted(set(labels_by_stem) - seen_stems)
    if orphans:
        raise DatasetSplitError(
            f"session {session_id!r} has orphan labels: {', '.join(orphans[:5])}"
        )
    return pairs


def _reject_orphan_flat_labels(session_dir: Path, image_files: list[Path], session_id: str) -> None:
    image_stems = {path.relative_to(session_dir).with_suffix("").as_posix() for path in image_files}
    for label_path in _files_under(session_dir, frozenset({".txt"})):
        relative = label_path.relative_to(session_dir)
        if relative.parts and relative.parts[0] in {"images", "labels"}:
            continue
        stem = relative.with_suffix("").as_posix()
        if stem not in image_stems:
            raise DatasetSplitError(
                f"session {session_id!r} has orphan label: {relative.as_posix()}"
            )


def _write_sessions_json(output: Path, plan: SessionSplitPlan) -> Path:
    # Directory-prefix keys match images/{split}/{session_id}/... for audit-dataset.
    mapping = {session_id: session_id for session_id in plan.all_sessions()}
    path = output / DEFAULT_SESSIONS_FILENAME
    if path.is_symlink():
        raise DatasetSplitError("sessions.json must not be a symlink")
    path.write_text(
        json.dumps(mapping, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return path.resolve()


def _files_under(directory: Path, extensions: frozenset[str]) -> list[Path]:
    files: list[Path] = []
    for current_root, directory_names, filenames in os.walk(directory, followlinks=False):
        current = Path(current_root)
        for name in sorted(directory_names):
            candidate = current / name
            if candidate.is_symlink():
                raise DatasetSplitError(f"symlinked directory is not allowed: {candidate}")
        directory_names[:] = sorted(
            name for name in directory_names if not (current / name).is_symlink()
        )
        for name in sorted(filenames):
            candidate = current / name
            if candidate.is_symlink():
                raise DatasetSplitError(f"symlinked file is not allowed: {candidate}")
            if candidate.suffix.lower() in extensions:
                files.append(candidate)
    return sorted(files)
