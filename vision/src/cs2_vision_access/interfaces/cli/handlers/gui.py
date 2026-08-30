"""gui subcommand — launch the desktop GUI dashboard for live outlining.

The GUI module is imported lazily inside the handler so the CLI parser stays
importable on machines without a display or tkinter.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.application.configuration import load_config
from cs2_vision_access.interfaces.cli.types import Subcommands
from cs2_vision_access.interfaces.gui.model import GuiSettings


def register_gui_command(subcommands: Subcommands) -> None:
    gui = subcommands.add_parser(
        "gui",
        help="launch the desktop GUI dashboard for live outlining",
        description=(
            "Launches the tkinter dashboard for the real-time live outline "
            "pipeline: capture source, model, outline style, overlay/output, "
            "and a run tab with live preview and diagnostics."
        ),
    )
    gui.add_argument(
        "--config",
        type=Path,
        default=None,
        metavar="PATH",
        help="path to a cs2-vision JSON config to pre-load into the GUI",
    )
    gui.set_defaults(handler=_handle_gui)


def _handle_gui(arguments: argparse.Namespace) -> int:
    # Lazy import keeps the parser importable without a display or tkinter.
    from cs2_vision_access.interfaces.gui.app import run_gui

    return run_gui(_settings_for(arguments))


def _settings_for(arguments: argparse.Namespace) -> GuiSettings:
    config_path = arguments.config
    if config_path is not None and Path(config_path).is_file():
        return GuiSettings.from_config(load_config(config_path))
    return GuiSettings()
