"""Parse, load, and aggregate participant ratings JSONL."""

from __future__ import annotations

import json
from collections.abc import Sequence
from pathlib import Path

from cs2_vision_access.workflows.study._util import (
    _mean,
    _optional_text,
    _path_segment,
    _required_text,
)
from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import (
    LIKERT_MAX,
    LIKERT_MIN,
    RATING_BOOL_FIELDS,
    RATING_LIKERT_FIELDS,
    RATING_SCHEMA_VERSION,
    RatingAggregate,
    StudyRating,
)

_RATING_REQUIRED = frozenset(
    {
        "schema_version",
        "participant_id",
        "session_id",
        "clip_id",
        "condition_id",
        "ratings",
    }
)
_RATING_OPTIONAL = frozenset({"notes", "stimulus_id", "presented_order"})
_RATING_ALLOWED = _RATING_REQUIRED | _RATING_OPTIONAL
_RATINGS_OBJECT_REQUIRED = frozenset(RATING_LIKERT_FIELDS) | frozenset(RATING_BOOL_FIELDS)


def parse_rating(raw: object) -> StudyRating:
    """Validate one ratings JSON object (JSONL line payload)."""
    if not isinstance(raw, dict):
        raise StudyError("rating root must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_RATING_REQUIRED - keys)
    unknown = sorted(keys - _RATING_ALLOWED)
    if missing or unknown:
        raise StudyError(f"rating keys do not match schema; missing={missing}, unknown={unknown}")

    schema_version = raw["schema_version"]
    if (
        isinstance(schema_version, bool)
        or not isinstance(schema_version, int)
        or schema_version != RATING_SCHEMA_VERSION
    ):
        raise StudyError(f"rating schema_version must be {RATING_SCHEMA_VERSION}")

    participant_id = _required_text(raw["participant_id"], "participant_id")
    session_id = _required_text(raw["session_id"], "session_id")
    clip_id = _path_segment(raw["clip_id"], label="clip_id")
    condition_id = _path_segment(raw["condition_id"], label="condition_id")
    notes = _optional_text(raw.get("notes"), "notes")
    stimulus_id = _optional_text(raw.get("stimulus_id"), "stimulus_id")
    presented_order = raw.get("presented_order")
    if presented_order is not None and (
        isinstance(presented_order, bool)
        or not isinstance(presented_order, int)
        or presented_order < 0
    ):
        raise StudyError("presented_order must be a non-negative integer or null")

    ratings_raw = raw["ratings"]
    if not isinstance(ratings_raw, dict):
        raise StudyError("ratings must be a JSON object")
    rating_keys = frozenset(ratings_raw)
    missing_r = sorted(_RATINGS_OBJECT_REQUIRED - rating_keys)
    unknown_r = sorted(rating_keys - _RATINGS_OBJECT_REQUIRED)
    if missing_r or unknown_r:
        raise StudyError(
            f"ratings object keys do not match protocol; missing={missing_r}, unknown={unknown_r}"
        )

    ratings: dict[str, object] = {}
    for field_name in RATING_LIKERT_FIELDS:
        value = ratings_raw[field_name]
        if (
            isinstance(value, bool)
            or not isinstance(value, int)
            or not LIKERT_MIN <= value <= LIKERT_MAX
        ):
            raise StudyError(
                f"ratings.{field_name} must be an integer in [{LIKERT_MIN}, {LIKERT_MAX}]"
            )
        ratings[field_name] = value
    for field_name in RATING_BOOL_FIELDS:
        value = ratings_raw[field_name]
        if not isinstance(value, bool):
            raise StudyError(f"ratings.{field_name} must be a boolean")
        ratings[field_name] = value

    return StudyRating(
        schema_version=schema_version,
        participant_id=participant_id,
        session_id=session_id,
        clip_id=clip_id,
        condition_id=condition_id,
        ratings=ratings,
        notes=notes,
        stimulus_id=stimulus_id,
        presented_order=presented_order,
    )


def load_ratings_jsonl(path: str | Path) -> tuple[StudyRating, ...]:
    """Load and validate ratings from a local JSONL file (blank lines skipped)."""
    ratings_path = Path(path)
    if ratings_path.is_symlink():
        raise StudyError("ratings path must not be a symlink")
    if not ratings_path.is_file():
        raise StudyError(f"ratings is not a regular file: {ratings_path}")
    if ratings_path.suffix.lower() not in {".jsonl", ".json"}:
        # Allow .json for single-object convenience only if content is JSONL-ish.
        raise StudyError("ratings file must use the .jsonl (or .json) extension")
    try:
        text = ratings_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise StudyError(f"could not read ratings: {error}") from error

    ratings: list[StudyRating] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            payload = json.loads(stripped)
        except json.JSONDecodeError as error:
            raise StudyError(f"{ratings_path}:{line_number}: invalid JSON: {error}") from error
        try:
            ratings.append(parse_rating(payload))
        except StudyError as error:
            raise StudyError(f"{ratings_path}:{line_number}: {error}") from error
    return tuple(ratings)


def aggregate_ratings(ratings: Sequence[StudyRating]) -> RatingAggregate:
    """Compute offline means and would-enable rates from validated ratings."""
    if not ratings:
        return RatingAggregate(
            schema_version=RATING_SCHEMA_VERSION,
            rating_count=0,
            participant_count=0,
            session_count=0,
            clip_ids=(),
            condition_ids=(),
            mean_likert={name: 0.0 for name in RATING_LIKERT_FIELDS},
            would_enable_rate=0.0,
            by_condition={},
        )

    participants = {rating.participant_id for rating in ratings}
    sessions = {rating.session_id for rating in ratings}
    clip_ids = tuple(sorted({rating.clip_id for rating in ratings}))
    condition_ids = tuple(sorted({rating.condition_id for rating in ratings}))

    mean_likert = {
        name: _mean([float(rating.ratings[name]) for rating in ratings])  # type: ignore[arg-type]
        for name in RATING_LIKERT_FIELDS
    }
    would_enable_rate = _mean(
        [1.0 if rating.ratings["would_enable"] else 0.0 for rating in ratings]
    )

    by_condition: dict[str, dict[str, object]] = {}
    for condition_id in condition_ids:
        subset = [rating for rating in ratings if rating.condition_id == condition_id]
        by_condition[condition_id] = {
            "rating_count": len(subset),
            "mean_likert": {
                name: _mean([float(item.ratings[name]) for item in subset])  # type: ignore[arg-type]
                for name in RATING_LIKERT_FIELDS
            },
            "would_enable_rate": _mean(
                [1.0 if item.ratings["would_enable"] else 0.0 for item in subset]
            ),
        }

    return RatingAggregate(
        schema_version=RATING_SCHEMA_VERSION,
        rating_count=len(ratings),
        participant_count=len(participants),
        session_count=len(sessions),
        clip_ids=clip_ids,
        condition_ids=condition_ids,
        mean_likert=mean_likert,
        would_enable_rate=would_enable_rate,
        by_condition=by_condition,
    )


def write_aggregate_json(aggregate: RatingAggregate, path: str | Path) -> Path:
    """Write an aggregate summary as sorted-key JSON."""
    destination = Path(path)
    if destination.suffix.lower() != ".json":
        raise StudyError("aggregate output must use the .json extension")
    if destination.is_symlink():
        raise StudyError("aggregate output must not be a symlink")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise StudyError("aggregate output parent must not be a symlink")
    text = json.dumps(aggregate.as_dict(), indent=2, sort_keys=True) + "\n"
    destination.write_text(text, encoding="utf-8")
    return destination.resolve()
