"""Human review workflow for draft masks produced by boxes-to-masks.

SAM / geometry drafts are never ground truth. Operators list, accept, reject,
and promote entries in ``draft_status.json`` (schema_version 1).

Per-file ``review_status`` values (schema_version 1, compatible extension):

- ``draft_pending`` — bootstrap default; not reviewed
- ``accepted`` — human accepted the draft polygon label
- ``rejected`` — human rejected; must not be promoted to GT layout
- ``promoted`` — accepted draft was copied into a GT labels directory

Promote / copy logic lives in :mod:`cs2_vision_access.labeling.promote`.
Shared draft_status I/O lives in :mod:`cs2_vision_access.labeling.review_io`.
"""

from __future__ import annotations

from collections.abc import Sequence
from pathlib import Path

from cs2_vision_access.labeling.review_io import (
    _OUTPUT_LABEL_KEY,
    _REVIEW_STATUS_KEY,
    _collect_stems,
    _files_list,
    _match_entries,
    _recompute_batch_review_status,
    _stem_from_output_label,
    load_draft_status,
    save_draft_status,
)
from cs2_vision_access.labeling.types import (
    REVIEW_STATUS_ACCEPTED,
    REVIEW_STATUS_DRAFT_PENDING,
    REVIEW_STATUS_REJECTED,
    SCHEMA_VERSION,
    BootstrapError,
)

# Re-export I/O so ``from ...review import load_draft_status`` keeps working.
__all__ = [
    "accept_drafts",
    "list_draft_status",
    "load_draft_status",
    "parse_stems_csv",
    "reject_drafts",
    "save_draft_status",
]


def list_draft_status(path: str | Path) -> dict[str, object]:
    """Return the draft_status document (normalized per-file statuses)."""
    return load_draft_status(path)


def accept_drafts(
    draft_status_path: str | Path,
    *,
    stem: str | None = None,
    stems: Sequence[str] | str | None = None,
    all_pending: bool = False,
) -> dict[str, object]:
    """Set review_status to ``accepted`` for selected stems; atomic rewrite."""
    path = Path(draft_status_path)
    payload = load_draft_status(path)
    files = _files_list(payload)

    selected_stems = _collect_stems(stem=stem, stems=stems)
    if all_pending:
        pending = [
            _stem_from_output_label(str(entry[_OUTPUT_LABEL_KEY]))
            for entry in files
            if entry.get(_REVIEW_STATUS_KEY) == REVIEW_STATUS_DRAFT_PENDING
        ]
        for item in pending:
            if item not in selected_stems:
                selected_stems.append(item)
    if not selected_stems:
        raise BootstrapError(
            "accept requires --stem, --stems, and/or --all-pending with pending drafts"
        )

    matched = _match_entries(files, selected_stems)
    updated: list[str] = []
    for key, entry in matched:
        entry[_REVIEW_STATUS_KEY] = REVIEW_STATUS_ACCEPTED
        updated.append(key)

    payload[_REVIEW_STATUS_KEY] = _recompute_batch_review_status(files)
    save_draft_status(path, payload)
    return {
        "action": "accept",
        "draft_status_path": str(path.resolve()),
        "updated_stems": updated,
        "updated_count": len(updated),
        "review_status": REVIEW_STATUS_ACCEPTED,
        "batch_review_status": payload[_REVIEW_STATUS_KEY],
        "schema_version": SCHEMA_VERSION,
    }


def reject_drafts(
    draft_status_path: str | Path,
    *,
    stem: str | None = None,
    stems: Sequence[str] | str | None = None,
) -> dict[str, object]:
    """Set review_status to ``rejected`` for selected stems; atomic rewrite."""
    path = Path(draft_status_path)
    payload = load_draft_status(path)
    files = _files_list(payload)

    selected_stems = _collect_stems(stem=stem, stems=stems)
    if not selected_stems:
        raise BootstrapError("reject requires --stem and/or --stems")

    matched = _match_entries(files, selected_stems)
    updated: list[str] = []
    for key, entry in matched:
        entry[_REVIEW_STATUS_KEY] = REVIEW_STATUS_REJECTED
        updated.append(key)

    payload[_REVIEW_STATUS_KEY] = _recompute_batch_review_status(files)
    save_draft_status(path, payload)
    return {
        "action": "reject",
        "draft_status_path": str(path.resolve()),
        "updated_stems": updated,
        "updated_count": len(updated),
        "review_status": REVIEW_STATUS_REJECTED,
        "batch_review_status": payload[_REVIEW_STATUS_KEY],
        "schema_version": SCHEMA_VERSION,
    }


def parse_stems_csv(value: str) -> list[str]:
    """Parse a comma-separated stems string for CLI use."""
    return _collect_stems(stems=value)
