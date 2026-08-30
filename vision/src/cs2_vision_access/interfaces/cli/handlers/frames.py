"""extract-frames subcommand."""

from __future__ import annotations

import argparse
from dataclasses import asdict
from pathlib import Path

from cs2_vision_access.interfaces.cli.common import _print_json
from cs2_vision_access.interfaces.cli.types import Subcommands
from cs2_vision_access.workflows.dataset.frames import extract_frames
from cs2_vision_access.workflows.dataset.frames.models import RightsPlaceholder


def register_frames_commands(subcommands: Subcommands) -> None:
    extract = subcommands.add_parser(
        "extract-frames", help="sample a recorded video into an empty annotation folder"
    )
    extract.add_argument("--input", type=Path, required=True)
    extract.add_argument("--output-directory", type=Path, required=True)
    extract.add_argument("--every-n-frames", type=int, default=30)
    extract.add_argument("--max-saved-frames", type=int, default=1000)
    extract.add_argument(
        "--session-id",
        help="provenance session_id written to session.json; defaults to output directory name",
    )
    extract.add_argument(
        "--notes",
        default="",
        dest="session_notes",
        help="free-text notes stored as capture_notes in session.json (default empty)",
    )
    extract.add_argument(
        "--rights-status",
        default="placeholder",
        help=(
            "Rights status (e.g. consented, public-domain, research-use-only; default placeholder)"
        ),
    )
    extract.add_argument(
        "--rights-source",
        default="",
        help="Source identifier (URL, DOI, or description of the footage origin)",
    )
    extract.add_argument(
        "--rights-consent",
        default="",
        help="Path or reference to a consent record or agreement",
    )
    extract.add_argument(
        "--rights-redistribution",
        default="",
        help="Redistribution terms (e.g. allowed, not-allowed, attribution-required)",
    )
    extract.add_argument(
        "--rights-retention",
        default="",
        help="Retention policy or expiry notes",
    )
    extract.set_defaults(handler=_handle_extract)


def _handle_extract(arguments: argparse.Namespace) -> int:
    rights = RightsPlaceholder(
        status=arguments.rights_status,
        source_identifier=arguments.rights_source,
        consent_record=arguments.rights_consent,
        redistribution=arguments.rights_redistribution,
        retention_notes=arguments.rights_retention,
    )
    summary = extract_frames(
        arguments.input,
        arguments.output_directory,
        every_n_frames=arguments.every_n_frames,
        max_saved_frames=arguments.max_saved_frames,
        session_id=arguments.session_id,
        capture_notes=arguments.session_notes,
        rights=rights,
    )
    _print_json(asdict(summary))
    return 0
