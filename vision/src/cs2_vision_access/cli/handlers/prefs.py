"""prefs show / set / reset subcommands."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.cli.style_args import (
    _add_style_arguments,
    _preferences_from_style_arguments,
    _resolved_style_dict,
)
from cs2_vision_access.prefs import (
    default_outline_preferences,
    load_outline_preferences,
    save_outline_preferences,
)
from cs2_vision_access.renderer import contrast_ratio


def register_prefs_commands(subcommands: argparse._SubParsersAction) -> None:
    prefs = subcommands.add_parser(
        "prefs",
        help="show, set, or reset local outline preferences JSON",
    )
    prefs_sub = prefs.add_subparsers(dest="prefs_command", required=True)

    prefs_show = prefs_sub.add_parser(
        "show",
        help="print local outline preferences as JSON",
    )
    prefs_show.add_argument(
        "--path",
        type=Path,
        required=True,
        metavar="PATH",
        help="local outline preferences .json path",
    )
    prefs_show.set_defaults(handler=_handle_prefs_show)

    prefs_set = prefs_sub.add_parser(
        "set",
        help=(
            "write local outline preferences; a named --outline-preset alone "
            "is enough (motor-friendly); other flags are optional overrides"
        ),
    )
    prefs_set.add_argument(
        "--path",
        type=Path,
        required=True,
        metavar="PATH",
        help="local outline preferences .json path",
    )
    prefs_set.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing preferences file",
    )
    _add_style_arguments(prefs_set, include_prefs=False)
    prefs_set.set_defaults(handler=_handle_prefs_set)

    prefs_reset = prefs_sub.add_parser(
        "reset",
        help="write default high-visibility outline preferences",
    )
    prefs_reset.add_argument(
        "--path",
        type=Path,
        required=True,
        metavar="PATH",
        help="local outline preferences .json path",
    )
    prefs_reset.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing preferences file",
    )
    prefs_reset.set_defaults(handler=_handle_prefs_reset)


def _handle_prefs_show(arguments: argparse.Namespace) -> int:
    preferences = load_outline_preferences(arguments.path)
    style = preferences.resolve()
    _print_json(
        {
            "path": str(Path(arguments.path).resolve()),
            "preferences": preferences.as_json(),
            "resolved_style": _resolved_style_dict(style),
            "stroke_contrast_ratio": round(
                contrast_ratio(style.inner_color, style.outer_color),
                2,
            ),
        }
    )
    return 0


def _handle_prefs_set(arguments: argparse.Namespace) -> int:
    preferences = _preferences_from_style_arguments(arguments)
    # Fail closed on style before writing (OutlinePreferences validates on construct).
    style = preferences.resolve()
    path = save_outline_preferences(
        arguments.path,
        preferences,
        overwrite=arguments.overwrite,
    )
    _print_json(
        {
            "path": str(path),
            "preferences": preferences.as_json(),
            "resolved_style": _resolved_style_dict(style),
            "stroke_contrast_ratio": round(
                contrast_ratio(style.inner_color, style.outer_color),
                2,
            ),
        }
    )
    return 0


def _handle_prefs_reset(arguments: argparse.Namespace) -> int:
    preferences = default_outline_preferences()
    path = save_outline_preferences(
        arguments.path,
        preferences,
        overwrite=arguments.overwrite,
    )
    style = preferences.resolve()
    _print_json(
        {
            "path": str(path),
            "preferences": preferences.as_json(),
            "resolved_style": _resolved_style_dict(style),
            "stroke_contrast_ratio": round(
                contrast_ratio(style.inner_color, style.outer_color),
                2,
            ),
        }
    )
    return 0
