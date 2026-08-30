"""Session-aware train/val plan builders for multi-source auto-training.

Treats each staging subdirectory (or video stem) as a whole ``session_id`` so
adjacent frames never cross the train/val boundary.
"""

from __future__ import annotations

import json
from pathlib import Path

from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS


def session_looks_valid(path: Path | str) -> bool:
    """Return True if *path* looks like a staging session directory.

    A session is valid if it has ``images/`` or ``labels/`` subdirs, or at least
    one top-level image (``IMAGE_EXTENSIONS``) or ``.txt`` label file.
    """
    root = Path(path)
    if not root.is_dir():
        return False
    if (root / "images").is_dir() or (root / "labels").is_dir():
        return True
    for child in root.iterdir():
        if not child.is_file():
            continue
        suffix = child.suffix.lower()
        if suffix in IMAGE_EXTENSIONS or suffix == ".txt":
            return True
    return False


def discover_session_ids(staging_root: Path | str) -> list[str]:
    """Return sorted session directory names under *staging_root*.

    Accepts both nested (``session/images|labels/``) and flat sessions
    (top-level images or ``.txt`` labels). Dot-directories are skipped.
    """
    root = Path(staging_root)
    if not root.is_dir():
        return []
    sessions: list[str] = []
    for child in sorted(root.iterdir()):
        if not child.is_dir() or child.name.startswith("."):
            continue
        if session_looks_valid(child):
            sessions.append(child.name)
    return sessions


def auto_split_sessions(
    session_ids: list[str],
    *,
    val_ratio: float = 0.2,
    seed: int = 42,
    min_val: int = 1,
) -> dict[str, list[str]]:
    """Partition whole sessions into train/val (deterministic hash order).

    Never splits frames within a session. Returns ``{"train": [...], "val": [...]}``.
    """
    if not session_ids:
        raise ValueError("session_ids must be non-empty")
    if not 0.0 < val_ratio < 1.0:
        raise ValueError("val_ratio must be in (0, 1)")

    # Stable order independent of filesystem: sort by (hash, name).
    ordered = sorted(session_ids, key=lambda s: (hash((seed, s)) & 0xFFFFFFFF, s))
    n = len(ordered)
    n_val = max(min_val, int(round(n * val_ratio)))
    if n_val >= n:
        n_val = max(1, n - 1) if n > 1 else 1
    val = ordered[:n_val]
    train = ordered[n_val:]
    if not train and n > 1:
        # Ensure at least one train session when possible.
        train = val[-1:]
        val = val[:-1]
    if not train:
        train = list(val)
    return {"train": train, "val": val}


def write_split_plan(
    plan: dict[str, list[str]],
    destination: Path | str,
) -> Path:
    """Write a JSON split plan compatible with ``assemble-dataset --plan``."""
    path = Path(destination)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "train": list(plan.get("train", [])),
        "val": list(plan.get("val", [])),
        "test": list(plan.get("test", [])),
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return path


def auto_plan_from_staging(
    staging_root: Path | str,
    plan_path: Path | str,
    *,
    val_ratio: float = 0.2,
    seed: int = 42,
) -> Path:
    """Discover sessions under *staging_root*, split, write plan JSON."""
    sessions = discover_session_ids(staging_root)
    if len(sessions) < 2:
        raise ValueError(
            f"need at least 2 session directories under {staging_root} for auto-split "
            f"(found {len(sessions)}: {sessions})"
        )
    plan = auto_split_sessions(sessions, val_ratio=val_ratio, seed=seed)
    return write_split_plan(plan, plan_path)


__all__ = [
    "auto_plan_from_staging",
    "auto_split_sessions",
    "discover_session_ids",
    "session_looks_valid",
    "write_split_plan",
]
