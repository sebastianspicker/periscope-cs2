"""Command-line interface for the file-only research workflow."""

from __future__ import annotations

import sys
from collections.abc import Sequence

from cs2_vision_access.cli.parser import build_parser
from cs2_vision_access.cli.style_args import _outline_style

__all__ = ["main", "build_parser", "_outline_style"]


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        return int(arguments.handler(arguments))
    except (ValueError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
