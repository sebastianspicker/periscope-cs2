"""Promote accepted draft labels into a ground-truth labels layout."""

from __future__ import annotations

from pathlib import Path

from cs2_vision_access.labeling.io import write_text_atomic
from cs2_vision_access.labeling.review_io import (
    _OUTPUT_LABEL_KEY,
    _REVIEW_STATUS_KEY,
    _files_list,
    _recompute_batch_review_status,
    _stem_from_output_label,
    load_draft_status,
    save_draft_status,
)
from cs2_vision_access.labeling.types import (
    DRAFT_STATUS_FILENAME,
    REVIEW_STATUS_ACCEPTED,
    REVIEW_STATUS_DRAFT_PENDING,
    REVIEW_STATUS_PROMOTED,
    REVIEW_STATUS_REJECTED,
    SCHEMA_VERSION,
    BootstrapError,
)


def promote_drafts(
    draft_status_path: str | Path,
    draft_labels_dir: str | Path,
    output_labels_dir: str | Path,
    *,
    only_accepted: bool = False,
) -> dict[str, object]:
    """Copy accepted draft ``.txt`` labels into a GT-layout labels directory.

    Never promotes ``draft_pending`` or ``rejected`` entries. When
    ``only_accepted`` is true, only ``accepted`` entries are copied (not
    already-``promoted``). Otherwise both ``accepted`` and ``promoted`` are
    eligible (re-copy). Each successfully copied accepted entry is marked
    ``promoted``.
    """
    status_path = Path(draft_status_path)
    draft_root = Path(draft_labels_dir)
    output_root = Path(output_labels_dir)

    if draft_root.is_symlink():
        raise BootstrapError("draft labels directory must not be a symlink")
    if not draft_root.is_dir():
        raise BootstrapError(f"draft labels directory is not a directory: {draft_root}")
    if output_root.is_symlink():
        raise BootstrapError("output labels directory must not be a symlink")
    if output_root.exists() and not output_root.is_dir():
        raise BootstrapError(f"output labels path exists and is not a directory: {output_root}")

    payload = load_draft_status(status_path)
    files = _files_list(payload)

    eligible_statuses = {REVIEW_STATUS_ACCEPTED}
    if not only_accepted:
        eligible_statuses.add(REVIEW_STATUS_PROMOTED)

    planned: list[tuple[str, dict[str, object], Path, Path]] = []
    skipped_pending = 0
    skipped_rejected = 0
    skipped_other = 0

    for entry in files:
        stem = _stem_from_output_label(str(entry[_OUTPUT_LABEL_KEY]))
        status = str(entry.get(_REVIEW_STATUS_KEY, REVIEW_STATUS_DRAFT_PENDING))
        if status == REVIEW_STATUS_DRAFT_PENDING:
            skipped_pending += 1
            continue
        if status == REVIEW_STATUS_REJECTED:
            skipped_rejected += 1
            continue
        if status not in eligible_statuses:
            skipped_other += 1
            continue
        rel_label = Path(str(entry[_OUTPUT_LABEL_KEY]))
        if rel_label.is_absolute() or ".." in rel_label.parts:
            raise BootstrapError(f"output_label must be a relative path without '..': {rel_label}")
        source = draft_root / rel_label
        destination = output_root / rel_label
        planned.append((stem, entry, source, destination))

    # Fail closed before writing: every source label must exist as a regular file.
    for stem, _entry, source, destination in planned:
        if source.is_symlink():
            raise BootstrapError(f"draft label must not be a symlink: {source}")
        if not source.is_file():
            raise BootstrapError(f"accepted draft label missing for stem {stem!r}: {source}")
        if destination.is_symlink():
            raise BootstrapError(f"output label must not be a symlink: {destination}")
        if destination.parent.is_symlink():
            raise BootstrapError(f"output label parent must not be a symlink: {destination.parent}")

    output_root.mkdir(parents=True, exist_ok=True)
    if output_root.is_symlink():
        raise BootstrapError("output labels directory must not be a symlink")

    promoted_stems: list[str] = []
    for stem, entry, source, destination in planned:
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            raise BootstrapError(f"could not read draft label {source}: {error}") from error
        destination.parent.mkdir(parents=True, exist_ok=True)
        write_text_atomic(destination, text)
        entry[_REVIEW_STATUS_KEY] = REVIEW_STATUS_PROMOTED
        promoted_stems.append(stem)

    payload[_REVIEW_STATUS_KEY] = _recompute_batch_review_status(files)
    save_draft_status(status_path, payload)

    return {
        "action": "promote",
        "draft_status_path": str(status_path.resolve()),
        "draft_labels_dir": str(draft_root.resolve()),
        "output_labels_dir": str(output_root.resolve()),
        "promoted_stems": promoted_stems,
        "promoted_count": len(promoted_stems),
        "skipped_pending": skipped_pending,
        "skipped_rejected": skipped_rejected,
        "skipped_other": skipped_other,
        "only_accepted": only_accepted,
        "batch_review_status": payload[_REVIEW_STATUS_KEY],
        "schema_version": SCHEMA_VERSION,
        "draft_status_filename": DRAFT_STATUS_FILENAME,
    }
