"""Dataclasses and public constants for user-study packages."""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path

from cs2_vision_access.renderer import OutlineStyle
from cs2_vision_access.segmenters import DEFAULT_SEGMENTER_BACKEND

SCHEMA_VERSION = 1
RATING_SCHEMA_VERSION = 1
CONDITION_KINDS = frozenset({"baseline", "outline"})
LIKERT_MIN = 1
LIKERT_MAX = 5
DEFAULT_MAX_FRAMES = 18_000

# Protocol scales from docs/USER_STUDY.md / ACCESSIBILITY design.
RATING_LIKERT_FIELDS = (
    "usefulness",
    "clutter",
    "comfort",
    "small_player_visibility",
    "error_confusion",
)
RATING_BOOL_FIELDS = ("would_enable",)


@dataclass(frozen=True)
class StudyClip:
    """One entitled local video clip in a study package."""

    clip_id: str
    input: str
    label: str | None = None
    notes: str | None = None
    tags: tuple[str, ...] = ()

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "clip_id": self.clip_id,
            "input": self.input,
        }
        if self.label is not None:
            payload["label"] = self.label
        if self.notes is not None:
            payload["notes"] = self.notes
        if self.tags:
            payload["tags"] = list(self.tags)
        return payload


@dataclass(frozen=True)
class StudyCondition:
    """One visual treatment (baseline passthrough or outline style)."""

    condition_id: str
    kind: str
    label: str | None = None
    description: str | None = None
    outline_preset: str | None = None
    prefs: str | None = None
    inner_color: str | None = None
    outer_color: str | None = None
    inner_width: int | None = None
    outer_width: int | None = None
    fill_opacity: float | None = None
    scale_with_frame: bool | None = None
    stroke_pattern: str | None = None
    dash_period_px: int | None = None

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "condition_id": self.condition_id,
            "kind": self.kind,
        }
        for key in (
            "label",
            "description",
            "outline_preset",
            "prefs",
            "inner_color",
            "outer_color",
            "inner_width",
            "outer_width",
            "fill_opacity",
            "scale_with_frame",
            "stroke_pattern",
            "dash_period_px",
        ):
            value = getattr(self, key)
            if value is not None:
                payload[key] = value
        return payload


@dataclass(frozen=True)
class StudyInference:
    """Shared segmenter settings for outline conditions in a package."""

    model: str
    manifest: str
    backend: str = DEFAULT_SEGMENTER_BACKEND
    class_names: tuple[str, ...] | None = None
    confidence: float = 0.45
    image_size: int = 640
    device: str = "cpu"
    max_frames: int = DEFAULT_MAX_FRAMES
    max_seconds: float | None = None

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "model": self.model,
            "manifest": self.manifest,
            "backend": self.backend,
            "confidence": self.confidence,
            "image_size": self.image_size,
            "device": self.device,
            "max_frames": self.max_frames,
        }
        if self.class_names is not None:
            payload["class_names"] = list(self.class_names)
        if self.max_seconds is not None:
            payload["max_seconds"] = self.max_seconds
        return payload


@dataclass(frozen=True)
class StudyPackage:
    """Schema-versioned study definition: clips × conditions (+ optional inference)."""

    schema_version: int
    study_id: str
    clips: tuple[StudyClip, ...]
    conditions: tuple[StudyCondition, ...]
    inference: StudyInference | None = None
    title: str | None = None
    description: str | None = None
    source_path: Path | None = field(default=None, compare=False, hash=False)

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "schema_version": self.schema_version,
            "study_id": self.study_id,
            "clips": [clip.as_json() for clip in self.clips],
            "conditions": [condition.as_json() for condition in self.conditions],
        }
        if self.title is not None:
            payload["title"] = self.title
        if self.description is not None:
            payload["description"] = self.description
        if self.inference is not None:
            payload["inference"] = self.inference.as_json()
        return payload

    @property
    def base_directory(self) -> Path:
        """Directory used to resolve relative clip/model/prefs paths."""
        if self.source_path is not None:
            return self.source_path.parent
        return Path.cwd()


@dataclass(frozen=True)
class StudyRenderJob:
    """One clip × condition unit of work for stimulus rendering."""

    study_id: str
    clip_id: str
    condition_id: str
    kind: str
    input_path: Path
    output_path: Path
    stimulus_id: str
    style: OutlineStyle | None = None
    max_frames: int = DEFAULT_MAX_FRAMES
    max_seconds: float | None = None

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "study_id": self.study_id,
            "clip_id": self.clip_id,
            "condition_id": self.condition_id,
            "kind": self.kind,
            "input_path": str(self.input_path),
            "output_path": str(self.output_path),
            "stimulus_id": self.stimulus_id,
            "max_frames": self.max_frames,
            "max_seconds": self.max_seconds,
        }
        if self.style is not None:
            payload["style"] = asdict(self.style)
        return payload


@dataclass(frozen=True)
class StudyRating:
    """One participant rating for a single clip × condition stimulus."""

    schema_version: int
    participant_id: str
    session_id: str
    clip_id: str
    condition_id: str
    ratings: dict[str, object]
    notes: str | None = None
    stimulus_id: str | None = None
    presented_order: int | None = None

    def as_json(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "schema_version": self.schema_version,
            "participant_id": self.participant_id,
            "session_id": self.session_id,
            "clip_id": self.clip_id,
            "condition_id": self.condition_id,
            "ratings": dict(self.ratings),
        }
        if self.notes is not None:
            payload["notes"] = self.notes
        if self.stimulus_id is not None:
            payload["stimulus_id"] = self.stimulus_id
        if self.presented_order is not None:
            payload["presented_order"] = self.presented_order
        return payload


@dataclass(frozen=True)
class RatingAggregate:
    """Offline summary of local ratings JSONL (no inference, no upload)."""

    schema_version: int
    rating_count: int
    participant_count: int
    session_count: int
    clip_ids: tuple[str, ...]
    condition_ids: tuple[str, ...]
    mean_likert: dict[str, float]
    would_enable_rate: float
    by_condition: dict[str, dict[str, object]]

    def as_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "rating_count": self.rating_count,
            "participant_count": self.participant_count,
            "session_count": self.session_count,
            "clip_ids": list(self.clip_ids),
            "condition_ids": list(self.condition_ids),
            "mean_likert": dict(self.mean_likert),
            "would_enable_rate": self.would_enable_rate,
            "by_condition": {key: dict(value) for key, value in self.by_condition.items()},
        }
