"""train-auto subcommand — config-driven multi-stage training orchestration."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from cs2_vision_access.interfaces.cli.common import _print_json
from cs2_vision_access.interfaces.cli.types import Subcommands
from cs2_vision_access.workflows.training.auto import (
    EXIT_FAIL,
    EXIT_HUMAN_GATE,
    EXIT_OK,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.workflows.training.auto.state import STAGE_ORDER


def register_train_auto_commands(subcommands: Subcommands) -> None:
    train_auto = subcommands.add_parser(
        "train-auto",
        help=(
            "run multi-stage auto-train from a JSON/YAML config "
            "(session_split or flat_cloud; resume via state.json)"
        ),
    )
    train_auto.add_argument(
        "--config",
        type=Path,
        required=True,
        help="path to train-auto config (.json / .yaml / .yml)",
    )
    train_auto.add_argument(
        "--force",
        action="store_true",
        help="ignore prior state.json and re-run all stages",
    )
    train_auto.add_argument(
        "--from-stage",
        choices=list(STAGE_ORDER),
        default=None,
        metavar="NAME",
        help=(f"resume starting at this stage (one of: {', '.join(STAGE_ORDER)})"),
    )
    train_auto.set_defaults(handler=_handle_train_auto)


def _handle_train_auto(arguments: argparse.Namespace) -> int:
    try:
        result = run_auto_train(
            arguments.config,
            force=arguments.force,
            from_stage=arguments.from_stage,
        )
    except AutoTrainError as error:
        print(f"error: {error}", file=sys.stderr)
        return EXIT_FAIL
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return EXIT_FAIL

    soft_notes = result.state.artifacts.get("soft_notes")
    if not isinstance(soft_notes, list):
        soft_notes = []
    report_status = result.state.artifacts.get("report_status")
    payload = {
        "exit_code": result.exit_code,
        "run_dir": str(result.run_dir),
        "status": result.state.status,
        "completed_stages": list(result.state.completed_stages),
        "message": result.message,
        "report": str(result.report_path) if result.report_path else None,
        "report_status": report_status,
        "soft_notes": soft_notes,
        "artifacts": {
            k: result.state.artifacts[k]
            for k in (
                "dataset_root",
                "onnx_model",
                "manifest",
                "package_zip",
            )
            if k in result.state.artifacts
        },
    }
    # Multi-iter unattended runs surface how many train→self-train cycles finished.
    st_iters = result.state.artifacts.get("self_train_iterations_completed")
    if st_iters is not None:
        payload["self_train_iterations_completed"] = st_iters
    _print_json(payload)

    if result.exit_code == EXIT_HUMAN_GATE:
        print(f"human_gate: {result.message}", file=sys.stderr)
        return EXIT_HUMAN_GATE
    if result.exit_code != EXIT_OK:
        return EXIT_FAIL
    return EXIT_OK
