"""Shared draft_status.json load/save and stem matching helpers."""

from __future__ import annotations

import json
from collections.abc import Sequence
from pathlib import Path

from cs2_vision_access.labeling.io import write_text_atomic
from cs2_vision_access.labeling.types import (
    REVIEW_STATUS_ACCEPTED,
    REVIEW_STATUS_DRAFT_PENDING,
    REVIEW_STATUS_PROMOTED,
    REVIEW_STATUS_REJECTED,
    REVIEW_STATUSES,
    SCHEMA_VERSION,
    BootstrapError,
)

# Keys written by boxes-to-masks that identify a draft label path.
_OUTPUT_LABEL_KEY = "output_label"
_REVIEW_STATUS_KEY = "review_status"


def load_draft_status(path: str | Path) -> dict[str, object]:
    """Load and lightly validate a draft_status.json document."""
    candidate = Path(path)
    if candidate.is_symlink():
        raise BootstrapError("draft status path must not be a symlink")
    if not candidate.is_file():
        raise BootstrapError(f"draft status is not a file: {candidate}")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BootstrapError(f"could not read draft status: {error}") from error
    if not isinstance(payload, dict):
        raise BootstrapError("draft status root must be a JSON object")
    version = payload.get("schema_version")
    if version != SCHEMA_VERSION:
        raise BootstrapError(
            f"unsupported draft_status schema_version {version!r}; expected {SCHEMA_VERSION}"
        )
    files = payload.get("files")
    if not isinstance(files, list):
        raise BootstrapError("draft status 'files' must be a list")
    # Normalize per-file review_status for older payloads that only had
    # batch-level review_status.
    batch_status = payload.get(_REVIEW_STATUS_KEY, REVIEW_STATUS_DRAFT_PENDING)
    if not isinstance(batch_status, str) or batch_status not in REVIEW_STATUSES:
        batch_status = REVIEW_STATUS_DRAFT_PENDING
    for index, entry in enumerate(files):
        if not isinstance(entry, dict):
            raise BootstrapError(f"draft status files[{index}] must be an object")
        if _OUTPUT_LABEL_KEY not in entry or not isinstance(entry[_OUTPUT_LABEL_KEY], str):
            raise BootstrapError(f"draft status files[{index}] missing string output_label")
        status = entry.get(_REVIEW_STATUS_KEY)
        if status is None:
            entry[_REVIEW_STATUS_KEY] = batch_status
        elif not isinstance(status, str) or status not in REVIEW_STATUSES:
            raise BootstrapError(
                f"draft status files[{index}] has invalid review_status {status!r}; "
                f"allowed: {', '.join(sorted(REVIEW_STATUSES))}"
            )
    return payload


def save_draft_status(path: str | Path, payload: dict[str, object]) -> None:
    """Atomically rewrite draft_status.json."""
    candidate = Path(path)
    if candidate.is_symlink():
        raise BootstrapError("draft status path must not be a symlink")
    if candidate.parent.is_symlink():
        raise BootstrapError("draft status parent must not be a symlink")
    write_text_atomic(
        candidate,
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
    )


def _stem_from_output_label(output_label: str) -> str:
    """Map ``output_label`` path (e.g. ``a/b.txt``) to stem key ``a/b``."""
    path = Path(output_label)
    if path.suffix.lower() == ".txt":
        return path.with_suffix("").as_posix()
    return path.as_posix()


def _normalize_stem(stem: str) -> str:
    text = stem.strip().replace("\\", "/")
    if not text:
        raise BootstrapError("stem must be non-empty")
    if text.endswith(".txt"):
        text = text[: -len(".txt")]
    # Drop a leading "./" for convenience.
    while text.startswith("./"):
        text = text[2:]
    if not text or text in {".", ".."} or ".." in text.split("/"):
        raise BootstrapError(f"invalid stem {stem!r}")
    return text


def _collect_stems(
    *,
    stem: str | None = None,
    stems: Sequence[str] | str | None = None,
) -> list[str]:
    collected: list[str] = []
    if stem is not None and str(stem).strip():
        collected.append(_normalize_stem(str(stem)))
    if stems is not None:
        if isinstance(stems, str):
            parts = [part.strip() for part in stems.split(",")]
        else:
            parts = []
            for item in stems:
                parts.extend(part.strip() for part in str(item).split(","))
        for part in parts:
            if not part:
                raise BootstrapError("stems list has an empty entry")
            collected.append(_normalize_stem(part))
    # Preserve order, drop duplicates.
    seen: set[str] = set()
    unique: list[str] = []
    for item in collected:
        if item not in seen:
            seen.add(item)
            unique.append(item)
    return unique


def _files_list(payload: dict[str, object]) -> list[dict[str, object]]:
    files = payload["files"]
    assert isinstance(files, list)
    return files  # type: ignore[return-value]


def _match_entries(
    files: list[dict[str, object]],
    stems: Sequence[str],
) -> list[tuple[str, dict[str, object]]]:
    by_stem: dict[str, dict[str, object]] = {}
    for entry in files:
        key = _stem_from_output_label(str(entry[_OUTPUT_LABEL_KEY]))
        if key in by_stem:
            raise BootstrapError(f"duplicate output_label stem in draft status: {key}")
        by_stem[key] = entry
    matched: list[tuple[str, dict[str, object]]] = []
    missing: list[str] = []
    for stem in stems:
        entry = by_stem.get(stem)
        if entry is None:
            missing.append(stem)
        else:
            matched.append((stem, entry))
    if missing:
        sample = ", ".join(missing[:5])
        extra = f" (+{len(missing) - 5} more)" if len(missing) > 5 else ""
        raise BootstrapError(f"unknown draft stem(s): {sample}{extra}")
    return matched


def _recompute_batch_review_status(files: list[dict[str, object]]) -> str:
    """Derive a coarse batch-level status from per-file statuses."""
    statuses = {str(entry.get(_REVIEW_STATUS_KEY, REVIEW_STATUS_DRAFT_PENDING)) for entry in files}
    if not statuses:
        return REVIEW_STATUS_DRAFT_PENDING
    if statuses == {REVIEW_STATUS_PROMOTED}:
        return REVIEW_STATUS_PROMOTED
    if statuses == {REVIEW_STATUS_ACCEPTED}:
        return REVIEW_STATUS_ACCEPTED
    if statuses == {REVIEW_STATUS_REJECTED}:
        return REVIEW_STATUS_REJECTED
    if statuses == {REVIEW_STATUS_DRAFT_PENDING}:
        return REVIEW_STATUS_DRAFT_PENDING
    # Mixed set: prefer pending if any remain, else accepted if any, else promoted.
    if REVIEW_STATUS_DRAFT_PENDING in statuses:
        return REVIEW_STATUS_DRAFT_PENDING
    if REVIEW_STATUS_ACCEPTED in statuses:
        return REVIEW_STATUS_ACCEPTED
    if REVIEW_STATUS_PROMOTED in statuses:
        return REVIEW_STATUS_PROMOTED
    return REVIEW_STATUS_REJECTED
