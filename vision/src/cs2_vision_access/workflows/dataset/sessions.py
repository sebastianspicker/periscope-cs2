"""Session mapping helpers for YOLO dataset audit and leakage checks."""

from __future__ import annotations

import json
from pathlib import Path

from cs2_vision_access.workflows.dataset._fs import files_under, relative
from cs2_vision_access.workflows.dataset.types import (
    DEFAULT_SESSIONS_FILENAME,
    IMAGE_EXTENSIONS,
    OPTIONAL_SPLITS,
    REQUIRED_SPLITS,
    SPLIT_NAMES,
    DatasetIssue,
)


def _resolve_sessions_path(root: Path, sessions_path: str | Path | None) -> Path | None:
    if sessions_path is not None:
        return Path(sessions_path)
    candidate = root / DEFAULT_SESSIONS_FILENAME
    # Include symlinks so _load_session_mapping can fail closed instead of skipping.
    if candidate.is_symlink() or candidate.is_file():
        return candidate
    return None


def _load_session_mapping(
    root: Path, sessions_path: Path
) -> tuple[dict[str, str] | None, list[DatasetIssue]]:
    rel = (
        relative(root, sessions_path)
        if sessions_path.is_relative_to(root)
        else sessions_path.as_posix()
    )
    if sessions_path.is_symlink():
        return None, [
            DatasetIssue(
                "SYMLINKED_PATH",
                rel,
                "symlinks are not allowed",
            )
        ]
    if not sessions_path.is_file():
        return None, [
            DatasetIssue(
                "MISSING_SESSIONS_FILE",
                rel,
                "sessions mapping file is missing",
            )
        ]
    try:
        payload = json.loads(sessions_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        return None, [
            DatasetIssue(
                "INVALID_SESSIONS_FILE",
                rel,
                f"sessions mapping could not be read: {error}",
            )
        ]

    if isinstance(payload, dict) and _looks_like_session_record(payload):
        payload = [payload]

    mapping: dict[str, str] = {}
    if isinstance(payload, dict):
        for key, value in payload.items():
            if not isinstance(key, str) or not key.strip():
                return None, [
                    DatasetIssue(
                        "INVALID_SESSIONS_FILE",
                        rel,
                        "session mapping keys must be non-empty strings",
                    )
                ]
            if not isinstance(value, str) or not value.strip():
                return None, [
                    DatasetIssue(
                        "INVALID_SESSIONS_FILE",
                        rel,
                        "session mapping values must be non-empty session_id strings",
                    )
                ]
            mapping[key.strip().replace("\\", "/")] = value.strip()
        return mapping, []

    if isinstance(payload, list):
        for index, entry in enumerate(payload):
            if not isinstance(entry, dict):
                return None, [
                    DatasetIssue(
                        "INVALID_SESSIONS_FILE",
                        rel,
                        f"sessions list entry {index} must be an object",
                    )
                ]
            session_id = entry.get("session_id")
            images = entry.get("images")
            if not isinstance(session_id, str) or not session_id.strip():
                return None, [
                    DatasetIssue(
                        "INVALID_SESSIONS_FILE",
                        rel,
                        f"sessions list entry {index} needs a non-empty session_id",
                    )
                ]
            if not isinstance(images, list) or not images:
                return None, [
                    DatasetIssue(
                        "INVALID_SESSIONS_FILE",
                        rel,
                        f"sessions list entry {index} needs a non-empty images list",
                    )
                ]
            for image_key in images:
                if not isinstance(image_key, str) or not image_key.strip():
                    return None, [
                        DatasetIssue(
                            "INVALID_SESSIONS_FILE",
                            rel,
                            f"sessions list entry {index} has an invalid image key",
                        )
                    ]
                normalized = image_key.strip().replace("\\", "/")
                previous = mapping.get(normalized)
                if previous is not None and previous != session_id.strip():
                    return None, [
                        DatasetIssue(
                            "INVALID_SESSIONS_FILE",
                            rel,
                            f"conflicting session_id for {normalized!r}",
                        )
                    ]
                mapping[normalized] = session_id.strip()
        return mapping, []

    return None, [
        DatasetIssue(
            "INVALID_SESSIONS_FILE",
            rel,
            "sessions root must be an object map, a session record, or a list of session objects",
        )
    ]


def _looks_like_session_record(payload: dict[object, object]) -> bool:
    return isinstance(payload.get("session_id"), str) and isinstance(payload.get("images"), list)


def _collect_split_images(root: Path) -> dict[str, tuple[Path, ...]]:
    collected: dict[str, tuple[Path, ...]] = {}
    images_root = root / "images"
    for split in REQUIRED_SPLITS + OPTIONAL_SPLITS:
        split_dir = images_root / split
        if not split_dir.is_dir() or split_dir.is_symlink():
            collected[split] = ()
            continue
        files, _symlinks = files_under(split_dir, IMAGE_EXTENSIONS)
        collected[split] = tuple(files)
    return collected


def _unique_basename_stems(
    images_by_split: dict[str, tuple[Path, ...]],
) -> frozenset[str]:
    counts: dict[str, int] = {}
    for image_paths in images_by_split.values():
        for image_path in image_paths:
            stem = image_path.stem
            counts[stem] = counts.get(stem, 0) + 1
    return frozenset(stem for stem, count in counts.items() if count == 1)


def _session_for_image(
    root: Path,
    image_path: Path,
    mapping: dict[str, str],
    *,
    unique_stems: frozenset[str],
) -> str | None:
    rel = relative(root, image_path)
    images_root = root / "images"
    try:
        under_images = image_path.relative_to(images_root).as_posix()
    except ValueError:
        under_images = rel
    split_relative = Path(under_images)
    parts = split_relative.parts
    split = parts[0] if parts else ""
    within_split = Path(*parts[1:]) if len(parts) > 1 else Path()
    stem_within = within_split.with_suffix("") if within_split.parts else Path()
    basename_stem = image_path.stem

    candidates: list[str] = [
        rel,
        under_images,
    ]
    if stem_within.parts:
        candidates.append(f"{split}/{stem_within.as_posix()}")
        # Prefer nested stem paths; bare basename only when dataset-wide unique.
        if len(stem_within.parts) > 1:
            candidates.append(stem_within.as_posix())
    if basename_stem in unique_stems:
        candidates.append(basename_stem)

    for key in candidates:
        if key and key in mapping:
            return mapping[key]

    # Directory / prefix keys: longest matching path prefix wins.
    # Bare split names (train/val/test) are ignored so they cannot shadow nested
    # session folders; use images/train/... if whole-split binding is intended.
    path_keys = [rel, under_images]
    best_key: str | None = None
    matched_session: str | None = None
    for path_key in path_keys:
        for map_key, session_id in mapping.items():
            if map_key in SPLIT_NAMES:
                continue
            if (path_key == map_key or path_key.startswith(f"{map_key}/")) and (
                best_key is None or len(map_key) > len(best_key)
            ):
                best_key = map_key
                matched_session = session_id
    if best_key is not None and matched_session is not None:
        return matched_session

    # Nested parent directory names as coarse session keys (skip the split segment).
    for part in split_relative.parts[1:-1]:
        if part in SPLIT_NAMES:
            continue
        if part in mapping:
            return mapping[part]
    return None
