"""CLI parser contracts that must not initialize a GUI, model, or capture device."""

from __future__ import annotations

import pytest

from cs2_vision_access.cli import build_parser


@pytest.mark.parametrize(
    "command",
    (
        "outline",
        "benchmark",
        "outline-presets",
        "prefs",
        "extract-frames",
        "validate-dataset",
        "audit-dataset",
        "assemble-dataset",
        "import-box-dataset",
        "boxes-to-masks",
        "review-drafts",
        "register-model",
        "train",
        "download-model",
        "setup",
        "live",
        "eval-masks",
        "export-predictions",
        "eval-negatives",
        "eval-temporal",
        "eval-comfort",
        "study-render",
        "study-aggregate",
        "bakeoff",
        "train-auto",
        "gui",
    ),
)
def test_each_top_level_command_has_help(command: str) -> None:
    with pytest.raises(SystemExit) as exited:
        build_parser().parse_args([command, "--help"])
    assert exited.value.code == 0


def test_cli_parser_errors_retain_exit_two() -> None:
    with pytest.raises(SystemExit) as exited:
        build_parser().parse_args(["not-a-command"])
    assert exited.value.code == 2


def test_download_default_is_checksum_pinned_registry_model() -> None:
    arguments = build_parser().parse_args(["download-model"])
    assert arguments.model_name == "yolo11n-seg"
