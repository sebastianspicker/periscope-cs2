"""Audit YOLO segmentation datasets: structure, session leakage, optional decode."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.dataset._fs import relative
from cs2_vision_access.dataset.sessions import (
    _collect_split_images,
    _load_session_mapping,
    _resolve_sessions_path,
    _session_for_image,
    _unique_basename_stems,
)
from cs2_vision_access.dataset.types import (
    OPTIONAL_SPLITS,
    REQUIRED_SPLITS,
    DatasetAuditResult,
    DatasetAuditSummary,
    DatasetIssue,
)
from cs2_vision_access.dataset.validate import validate_yolo_segmentation_dataset

_LEAKAGE_SAMPLE_LIMIT = 3


def audit_yolo_segmentation_dataset(
    dataset_root: str | Path,
    class_count: int,
    *,
    sessions_path: str | Path | None = None,
    check_decode: bool = False,
) -> DatasetAuditResult:
    """Audit structural integrity, optional session leakage, and optional image decode.

    When ``sessions_path`` is omitted, ``<root>/sessions.json`` is used if present
    (including when that path is a symlink, which fails closed). Session mapping
    accepts either:

    * an object mapping relative image path, split-relative path, stem path, unique
      basename stem, or directory prefix to ``session_id``;
    * a list of ``{"session_id": ..., "images": [...]}`` objects (a single such
      object is also accepted);
    * path/directory keys are preferred; bare basename stems apply only when the
      stem is unique across the whole dataset.

    The same ``session_id`` must not appear in both train and val. Optional test split
    is also checked against train and val. Image decode requires OpenCV and is skipped
    when OpenCV is unavailable (``decode_skipped_reason`` records that case).
    """
    root = Path(dataset_root)
    validation = validate_yolo_segmentation_dataset(root, class_count)
    issues: list[DatasetIssue] = list(validation.issues)
    session_mapped_image_count = 0
    train_session_count = 0
    val_session_count = 0
    leaked_session_count = 0
    decode_checked_count = 0
    decode_skipped_reason: str | None = None

    resolved_sessions = _resolve_sessions_path(root, sessions_path)
    mapping: dict[str, str] | None = None
    if resolved_sessions is not None:
        mapping, mapping_issues = _load_session_mapping(root, resolved_sessions)
        issues.extend(mapping_issues)

    images_by_split = _collect_split_images(root)
    unique_stems = _unique_basename_stems(images_by_split)
    if mapping is not None:
        sessions_by_split: dict[str, set[str]] = {
            split: set() for split in REQUIRED_SPLITS + OPTIONAL_SPLITS
        }
        paths_by_session_split: dict[str, dict[str, list[str]]] = {}
        for split, image_paths in images_by_split.items():
            for image_path in image_paths:
                rel = relative(root, image_path)
                session_id = _session_for_image(
                    root, image_path, mapping, unique_stems=unique_stems
                )
                if session_id is None:
                    issues.append(
                        DatasetIssue(
                            "UNMAPPED_IMAGE",
                            rel,
                            "image has no session_id in the sessions mapping",
                        )
                    )
                    continue
                session_mapped_image_count += 1
                sessions_by_split[split].add(session_id)
                paths_by_session_split.setdefault(session_id, {}).setdefault(split, []).append(rel)

        train_sessions = sessions_by_split["train"]
        val_sessions = sessions_by_split["val"]
        test_sessions = sessions_by_split.get("test", set())
        train_session_count = len(train_sessions)
        val_session_count = len(val_sessions)
        for split_a, split_b, intersection in (
            ("train", "val", train_sessions & val_sessions),
            ("train", "test", train_sessions & test_sessions),
            ("val", "test", val_sessions & test_sessions),
        ):
            for session_id in sorted(intersection):
                issues.append(
                    DatasetIssue(
                        "SESSION_LEAKAGE",
                        ".",
                        _session_leakage_message(
                            session_id,
                            split_a,
                            split_b,
                            paths_by_session_split.get(session_id, {}),
                        ),
                    )
                )
        leaked_session_count = len(
            (train_sessions & val_sessions)
            | (train_sessions & test_sessions)
            | (val_sessions & test_sessions)
        )

    if check_decode:
        decoder = _image_decoder()
        if decoder is None:
            decode_skipped_reason = "opencv_unavailable"
        else:
            for split in REQUIRED_SPLITS + OPTIONAL_SPLITS:
                for image_path in images_by_split.get(split, ()):
                    decode_checked_count += 1
                    if not decoder(image_path):
                        issues.append(
                            DatasetIssue(
                                "UNREADABLE_IMAGE",
                                relative(root, image_path),
                                "OpenCV could not decode image",
                            )
                        )

    issues.sort(
        key=lambda issue: (
            issue.path,
            issue.line is None,
            issue.line or 0,
            issue.code,
            issue.message,
        )
    )
    summary = DatasetAuditSummary(
        validation.summary.image_count,
        validation.summary.label_count,
        validation.summary.negative_label_count,
        validation.summary.annotation_count,
        len(issues),
        session_mapped_image_count,
        train_session_count,
        val_session_count,
        leaked_session_count,
        decode_checked_count,
        decode_skipped_reason,
    )
    return DatasetAuditResult(tuple(issues), summary)


def _session_leakage_message(
    session_id: str,
    split_a: str,
    split_b: str,
    paths_by_split: dict[str, list[str]],
) -> str:
    sample_a = _sample_paths(paths_by_split.get(split_a, ()))
    sample_b = _sample_paths(paths_by_split.get(split_b, ()))
    return (
        f"session_id {session_id!r} appears in both {split_a} and {split_b} "
        f"({split_a}: {sample_a}; {split_b}: {sample_b})"
    )


def _sample_paths(paths: list[str] | tuple[str, ...]) -> str:
    ordered = sorted(paths)
    if not ordered:
        return "(none)"
    shown = ordered[:_LEAKAGE_SAMPLE_LIMIT]
    text = ", ".join(shown)
    remaining = len(ordered) - len(shown)
    if remaining > 0:
        return f"{text}, ... (+{remaining} more)"
    return text


def _image_decoder() -> Callable[[Path], bool] | None:
    try:
        import cv2  # type: ignore
    except ImportError:
        return None

    def decode(path: Path) -> bool:
        image = cv2.imread(str(path), cv2.IMREAD_COLOR)
        return image is not None

    return decode
