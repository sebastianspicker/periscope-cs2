"""Public dataclasses and constants for YOLO segmentation datasets."""

from __future__ import annotations

from dataclasses import dataclass

IMAGE_EXTENSIONS = frozenset({".jpg", ".jpeg", ".png", ".bmp", ".webp"})
REQUIRED_SPLITS = ("train", "val")
OPTIONAL_SPLITS = ("test",)
SPLIT_NAMES = frozenset(REQUIRED_SPLITS + OPTIONAL_SPLITS)
DEFAULT_SESSIONS_FILENAME = "sessions.json"


@dataclass(frozen=True)
class DatasetIssue:
    """One deterministic validation failure, using a dataset-relative path."""

    code: str
    path: str
    message: str
    line: int | None = None


@dataclass(frozen=True)
class DatasetSummary:
    image_count: int
    label_count: int
    negative_label_count: int
    annotation_count: int
    issue_count: int


@dataclass(frozen=True)
class DatasetValidationResult:
    issues: tuple[DatasetIssue, ...]
    summary: DatasetSummary

    @property
    def is_valid(self) -> bool:
        return not self.issues


@dataclass(frozen=True)
class DatasetAuditSummary:
    image_count: int
    label_count: int
    negative_label_count: int
    annotation_count: int
    issue_count: int
    session_mapped_image_count: int
    train_session_count: int
    val_session_count: int
    leaked_session_count: int
    decode_checked_count: int
    decode_skipped_reason: str | None = None


@dataclass(frozen=True)
class DatasetAuditResult:
    issues: tuple[DatasetIssue, ...]
    summary: DatasetAuditSummary

    @property
    def is_valid(self) -> bool:
        return not self.issues
