"""Argument parser construction for the cs2-vision CLI."""

from __future__ import annotations

import argparse

from cs2_vision_access import __version__
from cs2_vision_access.interfaces.cli.handlers.bakeoff import register_bakeoff_commands
from cs2_vision_access.interfaces.cli.handlers.dataset import register_dataset_commands
from cs2_vision_access.interfaces.cli.handlers.download_model import (
    register_download_model_command,
)
from cs2_vision_access.interfaces.cli.handlers.eval import register_eval_commands
from cs2_vision_access.interfaces.cli.handlers.frames import register_frames_commands
from cs2_vision_access.interfaces.cli.handlers.gui import register_gui_command
from cs2_vision_access.interfaces.cli.handlers.live import register_live_commands
from cs2_vision_access.interfaces.cli.handlers.model import register_model_commands
from cs2_vision_access.interfaces.cli.handlers.outline import register_outline_commands
from cs2_vision_access.interfaces.cli.handlers.prefs import register_prefs_commands
from cs2_vision_access.interfaces.cli.handlers.review_drafts import register_review_drafts_commands
from cs2_vision_access.interfaces.cli.handlers.setup import register_setup_command
from cs2_vision_access.interfaces.cli.handlers.study import register_study_commands
from cs2_vision_access.interfaces.cli.handlers.train_auto import register_train_auto_commands


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="cs2-vision",
        description=(
            "Accessibility research for outlining visible players in Counter-Strike 2 "
            "footage. Supports offline file processing and live HDMI/SDI capture."
        ),
    )
    parser.add_argument("--version", action="version", version=__version__)
    subcommands = parser.add_subparsers(dest="command", required=True)

    register_outline_commands(subcommands)
    register_prefs_commands(subcommands)
    register_frames_commands(subcommands)
    register_dataset_commands(subcommands)
    register_review_drafts_commands(subcommands)
    register_model_commands(subcommands)
    register_download_model_command(subcommands)
    register_setup_command(subcommands)
    register_live_commands(subcommands)
    register_eval_commands(subcommands)
    register_study_commands(subcommands)
    register_bakeoff_commands(subcommands)
    register_train_auto_commands(subcommands)
    register_gui_command(subcommands)
    return parser
