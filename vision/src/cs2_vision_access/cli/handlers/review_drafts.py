"""review-drafts list / accept / reject / promote subcommands."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.labeling import (
    accept_drafts,
    list_draft_status,
    promote_drafts,
    reject_drafts,
)


def register_review_drafts_commands(subcommands: argparse._SubParsersAction) -> None:
    review = subcommands.add_parser(
        "review-drafts",
        help=(
            "list, accept, reject, or promote draft masks from boxes-to-masks "
            "(never auto-GT; human review required)"
        ),
    )
    review_sub = review.add_subparsers(dest="review_drafts_command", required=True)

    list_cmd = review_sub.add_parser(
        "list",
        help="print draft_status.json as JSON (normalized per-file review_status)",
    )
    list_cmd.add_argument(
        "--draft-status",
        type=Path,
        required=True,
        metavar="PATH",
        help="path to draft_status.json written by boxes-to-masks",
    )
    list_cmd.set_defaults(handler=_handle_list)

    accept_cmd = review_sub.add_parser(
        "accept",
        help="set review_status to accepted for selected stems (atomic rewrite)",
    )
    accept_cmd.add_argument(
        "--draft-status",
        type=Path,
        required=True,
        metavar="PATH",
        help="path to draft_status.json",
    )
    accept_cmd.add_argument(
        "--stem",
        default=None,
        metavar="NAME",
        help="single draft stem (output_label without .txt)",
    )
    accept_cmd.add_argument(
        "--stems",
        default=None,
        metavar="A,B",
        help="comma-separated draft stems",
    )
    accept_cmd.add_argument(
        "--all-pending",
        action="store_true",
        help="accept every entry still in draft_pending",
    )
    accept_cmd.set_defaults(handler=_handle_accept)

    reject_cmd = review_sub.add_parser(
        "reject",
        help="set review_status to rejected for selected stems (atomic rewrite)",
    )
    reject_cmd.add_argument(
        "--draft-status",
        type=Path,
        required=True,
        metavar="PATH",
        help="path to draft_status.json",
    )
    reject_cmd.add_argument(
        "--stem",
        default=None,
        metavar="NAME",
        help="single draft stem (output_label without .txt)",
    )
    reject_cmd.add_argument(
        "--stems",
        default=None,
        metavar="A,B",
        help="comma-separated draft stems",
    )
    reject_cmd.set_defaults(handler=_handle_reject)

    promote_cmd = review_sub.add_parser(
        "promote",
        help=(
            "copy accepted draft .txt labels into a GT labels directory; "
            "never promotes draft_pending or rejected"
        ),
    )
    promote_cmd.add_argument(
        "--draft-status",
        type=Path,
        required=True,
        metavar="PATH",
        help="path to draft_status.json",
    )
    promote_cmd.add_argument(
        "--draft-labels-dir",
        type=Path,
        required=True,
        metavar="PATH",
        help="directory of draft YOLO-seg polygon labels (boxes-to-masks output)",
    )
    promote_cmd.add_argument(
        "--output-labels-dir",
        type=Path,
        required=True,
        metavar="PATH",
        help="destination labels directory (GT layout; mirrors nested relative paths)",
    )
    promote_cmd.add_argument(
        "--only-accepted",
        action="store_true",
        help=("promote only accepted entries (exclude already-promoted re-copies)"),
    )
    promote_cmd.set_defaults(handler=_handle_promote)


def _handle_list(arguments: argparse.Namespace) -> int:
    payload = list_draft_status(arguments.draft_status)
    _print_json(payload)
    return 0


def _handle_accept(arguments: argparse.Namespace) -> int:
    summary = accept_drafts(
        arguments.draft_status,
        stem=arguments.stem,
        stems=arguments.stems,
        all_pending=arguments.all_pending,
    )
    _print_json(summary)
    return 0


def _handle_reject(arguments: argparse.Namespace) -> int:
    summary = reject_drafts(
        arguments.draft_status,
        stem=arguments.stem,
        stems=arguments.stems,
    )
    _print_json(summary)
    return 0


def _handle_promote(arguments: argparse.Namespace) -> int:
    summary = promote_drafts(
        arguments.draft_status,
        arguments.draft_labels_dir,
        arguments.output_labels_dir,
        only_accepted=arguments.only_accepted,
    )
    _print_json(summary)
    return 0
