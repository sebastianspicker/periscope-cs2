"""Whole-session split plan types and builders for YOLO dataset assembly."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.dataset.types import SPLIT_NAMES


class DatasetSplitError(ValueError):
    """Session plan or staging tree cannot be assembled safely."""


@dataclass(frozen=True)
class SessionSplitPlan:
    """Whole-session assignment to YOLO splits."""

    train: tuple[str, ...]
    val: tuple[str, ...]
    test: tuple[str, ...] = ()

    def sessions_for(self, split: str) -> tuple[str, ...]:
        if split == "train":
            return self.train
        if split == "val":
            return self.val
        if split == "test":
            return self.test
        raise DatasetSplitError(f"unknown split {split!r}")

    def all_sessions(self) -> tuple[str, ...]:
        return self.train + self.val + self.test

    def as_dict(self) -> dict[str, list[str]]:
        payload: dict[str, list[str]] = {
            "train": list(self.train),
            "val": list(self.val),
        }
        if self.test:
            payload["test"] = list(self.test)
        return payload


def load_split_plan(path: str | Path) -> SessionSplitPlan:
    """Load a JSON plan mapping splits to whole session_id lists."""
    plan_path = Path(path)
    if plan_path.is_symlink():
        raise DatasetSplitError("split plan must not be a symlink")
    if not plan_path.is_file():
        raise DatasetSplitError(f"split plan is not a regular file: {plan_path}")
    try:
        payload = json.loads(plan_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise DatasetSplitError(f"could not read split plan: {error}") from error
    if not isinstance(payload, dict):
        raise DatasetSplitError("split plan root must be a JSON object")
    return build_split_plan(
        train=_coerce_session_list(payload.get("train"), "train"),
        val=_coerce_session_list(payload.get("val"), "val"),
        test=_coerce_session_list(payload.get("test"), "test"),
    )


def build_split_plan(
    *,
    train: list[str] | tuple[str, ...] | None,
    val: list[str] | tuple[str, ...] | None,
    test: list[str] | tuple[str, ...] | None = None,
) -> SessionSplitPlan:
    """Validate and normalise whole-session split assignment."""
    train_ids = _normalise_session_ids(train or (), "train")
    val_ids = _normalise_session_ids(val or (), "val")
    test_ids = _normalise_session_ids(test or (), "test")
    if not train_ids:
        raise DatasetSplitError("train must list at least one session_id")
    if not val_ids:
        raise DatasetSplitError("val must list at least one session_id")

    ownership: dict[str, str] = {}
    for split, session_ids in (
        ("train", train_ids),
        ("val", val_ids),
        ("test", test_ids),
    ):
        for session_id in session_ids:
            previous = ownership.get(session_id)
            if previous is not None:
                raise DatasetSplitError(
                    f"session_id {session_id!r} is assigned to both "
                    f"{previous} and {split}; whole sessions must stay disjoint"
                )
            ownership[session_id] = split
    return SessionSplitPlan(train=train_ids, val=val_ids, test=test_ids)


def _coerce_session_list(value: object, split: str) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [part for part in (item.strip() for item in value.split(",")) if part]
    if not isinstance(value, list):
        raise DatasetSplitError(f"split plan {split!r} must be a list of session_id strings")
    sessions: list[str] = []
    for index, item in enumerate(value):
        if not isinstance(item, str) or not item.strip():
            raise DatasetSplitError(
                f"split plan {split!r} entry {index} must be a non-empty string"
            )
        sessions.append(item.strip())
    return sessions


def _normalise_session_ids(values: list[str] | tuple[str, ...], split: str) -> tuple[str, ...]:
    seen: set[str] = set()
    ordered: list[str] = []
    for raw in values:
        for part in raw.split(","):
            session_id = part.strip()
            if not session_id:
                continue
            if any(separator in session_id for separator in ("/", "\\")):
                raise DatasetSplitError(f"session_id {session_id!r} must be a single path segment")
            if session_id in SPLIT_NAMES:
                raise DatasetSplitError(f"session_id {session_id!r} collides with a split name")
            if session_id in seen:
                raise DatasetSplitError(
                    f"duplicate session_id {session_id!r} in {split} assignment"
                )
            seen.add(session_id)
            ordered.append(session_id)
    return tuple(ordered)
