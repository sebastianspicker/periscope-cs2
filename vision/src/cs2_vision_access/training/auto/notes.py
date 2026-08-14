"""Soft-note helpers for auto-train stage updates."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any


def append_soft_note(updates: dict[str, Any], note: str) -> None:
    """Append *note* to updates['soft_notes'] (list)."""
    notes = updates.setdefault("soft_notes", [])
    if isinstance(notes, list):
        notes.append(note)
    else:
        updates["soft_notes"] = [note]


def merge_soft_notes(*sources: Mapping[str, Any] | Sequence[Any] | None) -> list[str]:
    """Collect soft_notes from dicts (key soft_notes) or plain lists into one list."""
    out: list[str] = []
    for src in sources:
        if src is None:
            continue
        if isinstance(src, Mapping):
            raw = src.get("soft_notes")
            if isinstance(raw, list):
                out.extend(str(x) for x in raw)
        elif isinstance(src, (list, tuple)):
            out.extend(str(x) for x in src)
    return out


def apply_merged_soft_notes(updates: dict[str, Any], *sources) -> None:
    """Merge soft notes from sources into updates via append_soft_note for each."""
    for note in merge_soft_notes(*sources):
        append_soft_note(updates, note)
