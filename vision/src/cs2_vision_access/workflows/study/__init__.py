"""User-study package: stimulus packs, ratings schema, offline aggregation.

Loads a schema-versioned ``study.json`` describing clips × outline conditions,
plans or renders a local stimulus pack (file-only), and validates/aggregates
JSONL ratings. No network upload, live capture, or participant identity service.
"""

from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import (
    CONDITION_KINDS,
    DEFAULT_MAX_FRAMES,
    LIKERT_MAX,
    LIKERT_MIN,
    RATING_BOOL_FIELDS,
    RATING_LIKERT_FIELDS,
    RATING_SCHEMA_VERSION,
    SCHEMA_VERSION,
    RatingAggregate,
    StudyClip,
    StudyCondition,
    StudyInference,
    StudyPackage,
    StudyRating,
    StudyRenderJob,
)
from cs2_vision_access.workflows.study.parse import (
    load_study_package,
    parse_study_package,
    resolve_condition_style,
)
from cs2_vision_access.workflows.study.ratings import (
    aggregate_ratings,
    load_ratings_jsonl,
    parse_rating,
    write_aggregate_json,
)
from cs2_vision_access.workflows.study.render import (
    plan_study_render,
    render_study_pack,
    stimulus_id_for,
)

__all__ = [
    "CONDITION_KINDS",
    "DEFAULT_MAX_FRAMES",
    "LIKERT_MAX",
    "LIKERT_MIN",
    "RATING_BOOL_FIELDS",
    "RATING_LIKERT_FIELDS",
    "RATING_SCHEMA_VERSION",
    "SCHEMA_VERSION",
    "RatingAggregate",
    "StudyClip",
    "StudyCondition",
    "StudyError",
    "StudyInference",
    "StudyPackage",
    "StudyRating",
    "StudyRenderJob",
    "aggregate_ratings",
    "load_ratings_jsonl",
    "load_study_package",
    "parse_rating",
    "parse_study_package",
    "plan_study_render",
    "render_study_pack",
    "resolve_condition_style",
    "stimulus_id_for",
    "write_aggregate_json",
]
