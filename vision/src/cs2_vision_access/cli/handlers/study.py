"""study-render and study-aggregate subcommands."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.study import (
    aggregate_ratings,
    load_ratings_jsonl,
    load_study_package,
    render_study_pack,
    write_aggregate_json,
)


def register_study_commands(subcommands: argparse._SubParsersAction) -> None:
    study_render = subcommands.add_parser(
        "study-render",
        help="batch-render study.json clip×condition stimuli into a local pack",
    )
    study_render.add_argument(
        "--study",
        type=Path,
        required=True,
        help="schema v1 study package JSON (clips, conditions, optional inference)",
    )
    study_render.add_argument(
        "--output-directory",
        type=Path,
        required=True,
        help="local directory for stimuli/, pack-manifest.v1.json, ratings template",
    )
    study_render.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing stimulus MP4s after successful per-job writes",
    )
    study_render.add_argument(
        "--validate-only",
        action="store_true",
        help=(
            "load and expand clip×condition jobs, write pack-manifest only; "
            "do not decode video or load models"
        ),
    )
    study_render.set_defaults(handler=_handle_study_render)

    study_aggregate = subcommands.add_parser(
        "study-aggregate",
        help="aggregate local ratings JSONL into offline summary statistics",
    )
    study_aggregate.add_argument(
        "--ratings",
        type=Path,
        required=True,
        help="schema v1 ratings JSONL (one participant×stimulus object per line)",
    )
    study_aggregate.add_argument(
        "--output",
        type=Path,
        help="optional path for schema-versioned aggregate JSON",
    )
    study_aggregate.set_defaults(handler=_handle_study_aggregate)


def _handle_study_render(arguments: argparse.Namespace) -> int:
    package = load_study_package(arguments.study)
    manifest = render_study_pack(
        package,
        arguments.output_directory,
        overwrite=arguments.overwrite,
        validate_only=arguments.validate_only,
    )
    _print_json(manifest)
    return 0


def _handle_study_aggregate(arguments: argparse.Namespace) -> int:
    ratings = load_ratings_jsonl(arguments.ratings)
    aggregate = aggregate_ratings(ratings)
    payload = aggregate.as_dict()
    if arguments.output is not None:
        path = write_aggregate_json(aggregate, arguments.output)
        payload = {**payload, "output": str(path)}
    _print_json(payload)
    return 0
