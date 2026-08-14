"""``python -m cs2_vision_access.training.auto --config PATH`` entry point."""

from __future__ import annotations

import argparse
import sys

from cs2_vision_access.training.auto import (
    EXIT_FAIL,
    EXIT_HUMAN_GATE,
    EXIT_OK,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.training.auto.state import STAGE_ORDER


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m cs2_vision_access.training.auto",
        description="Config-driven multi-stage CS2 auto-train orchestration",
    )
    parser.add_argument(
        "--config",
        required=True,
        help="path to train-auto JSON/YAML config",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="ignore prior state.json and re-run all stages",
    )
    parser.add_argument(
        "--from-stage",
        choices=list(STAGE_ORDER),
        default=None,
        help="resume starting at this stage (invalidates later completions)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        result = run_auto_train(
            args.config,
            force=args.force,
            from_stage=args.from_stage,
        )
    except AutoTrainError as error:
        print(f"error: {error}", file=sys.stderr)
        return EXIT_FAIL
    except Exception as error:  # pragma: no cover - unexpected
        print(f"error: {error}", file=sys.stderr)
        return EXIT_FAIL

    if result.exit_code == EXIT_OK:
        print(f"auto-train completed: {result.run_dir}")
        if result.report_path is not None:
            print(f"report: {result.report_path}")
    elif result.exit_code == EXIT_HUMAN_GATE:
        print(f"human_gate: {result.message}", file=sys.stderr)
    return int(result.exit_code)


if __name__ == "__main__":
    raise SystemExit(main())
