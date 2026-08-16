"""bakeoff subcommand: offline multi-backend latency/count comparison."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.bakeoff import (
    DEFAULT_MAX_FRAMES,
    DEFAULT_OUTPUT,
    BakeoffError,
    load_bakeoff_config,
    parse_backends_csv,
    run_bakeoff,
    specs_from_cli_pairs,
    specs_from_config,
)
from cs2_vision_access.cli.common import _print_json


def register_bakeoff_commands(subcommands: argparse._SubParsersAction) -> None:
    bakeoff = subcommands.add_parser(
        "bakeoff",
        help=(
            "run sequential offline backend comparison (latency/counts) "
            "on one local video; no winner is declared"
        ),
    )
    bakeoff.add_argument(
        "--input",
        type=Path,
        required=True,
        help="local recorded video path (file-only)",
    )
    bakeoff.add_argument(
        "--config",
        type=Path,
        help=(
            "JSON listing backends with model/manifest pairs "
            "(optional shared max_frames, confidence, device, ...)"
        ),
    )
    bakeoff.add_argument(
        "--backends",
        type=str,
        help=(
            "comma-separated backends (e.g. ultralytics-onnx,rfdetr); "
            "pairs with --model-a/--manifest-a and --model-b/--manifest-b"
        ),
    )
    bakeoff.add_argument("--model-a", type=Path, help="model path for first backend")
    bakeoff.add_argument("--manifest-a", type=Path, help="manifest path for first backend")
    bakeoff.add_argument("--model-b", type=Path, help="model path for second backend")
    bakeoff.add_argument("--manifest-b", type=Path, help="manifest path for second backend")
    bakeoff.add_argument(
        "--max-frames",
        type=int,
        default=None,
        help=f"hard processing bound; default: {DEFAULT_MAX_FRAMES}",
    )
    bakeoff.add_argument(
        "--max-seconds",
        type=float,
        default=None,
        help="optional source-video duration bound",
    )
    bakeoff.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"schema v1 bakeoff JSON path; default: {DEFAULT_OUTPUT}",
    )
    bakeoff.add_argument("--class-name", action="append")
    bakeoff.add_argument("--confidence", type=float, default=None)
    bakeoff.add_argument("--image-size", type=int, default=None)
    bakeoff.add_argument("--device", default=None)
    bakeoff.set_defaults(handler=_handle_bakeoff)


def _handle_bakeoff(arguments: argparse.Namespace) -> int:
    config_path: Path | None = arguments.config
    backends_csv: str | None = arguments.backends

    if config_path is not None and backends_csv is not None:
        raise BakeoffError("use either --config or --backends, not both")
    if config_path is None and backends_csv is None:
        raise BakeoffError(
            "bakeoff requires --config or --backends "
            "(with matching --model-a/--manifest-a and optional -b pairs)"
        )

    max_frames = arguments.max_frames if arguments.max_frames is not None else DEFAULT_MAX_FRAMES
    max_seconds = arguments.max_seconds
    class_names = tuple(arguments.class_name) if arguments.class_name is not None else None
    confidence = arguments.confidence
    image_size = arguments.image_size
    device = arguments.device

    if config_path is not None:
        payload = load_bakeoff_config(config_path)
        base = config_path.parent
        specs = specs_from_config(payload, base_directory=base)
        if arguments.max_frames is None and "max_frames" in payload:
            raw_mf = payload["max_frames"]
            if isinstance(raw_mf, bool) or not isinstance(raw_mf, int) or raw_mf <= 0:
                raise BakeoffError("config max_frames must be a positive integer")
            max_frames = raw_mf
        if arguments.max_seconds is None and "max_seconds" in payload:
            raw_ms = payload["max_seconds"]
            if raw_ms is not None and (
                isinstance(raw_ms, bool) or not isinstance(raw_ms, (int, float))
            ):
                raise BakeoffError("config max_seconds must be a number or null")
            max_seconds = float(raw_ms) if raw_ms is not None else None
        if confidence is None and "confidence" in payload:
            raw_c = payload["confidence"]
            if isinstance(raw_c, bool) or not isinstance(raw_c, (int, float)):
                raise BakeoffError("config confidence must be a number")
            confidence = float(raw_c)
        if image_size is None and "image_size" in payload:
            raw_is = payload["image_size"]
            if isinstance(raw_is, bool) or not isinstance(raw_is, int) or raw_is <= 0:
                raise BakeoffError("config image_size must be a positive integer")
            image_size = raw_is
        if device is None and "device" in payload:
            raw_d = payload["device"]
            if not isinstance(raw_d, str) or not raw_d.strip():
                raise BakeoffError("config device must be a non-empty string")
            device = raw_d.strip()
    else:
        if backends_csv is None:
            raise BakeoffError("bakeoff requires --backends when --config is absent")
        names = parse_backends_csv(backends_csv)
        specs = specs_from_cli_pairs(
            names,
            model_a=arguments.model_a,
            manifest_a=arguments.manifest_a,
            model_b=arguments.model_b,
            manifest_b=arguments.manifest_b,
            confidence=confidence if confidence is not None else 0.45,
            image_size=image_size if image_size is not None else 640,
            device=device if device is not None else "cpu",
            class_names=class_names,
        )
        # Specs already carry confidence/image_size/device/class_names; do not
        # re-apply shared overrides that would double-write defaults.
        confidence = None
        image_size = None
        device = None
        class_names = None

    report = run_bakeoff(
        input_path=arguments.input,
        backends=specs,
        max_frames=max_frames,
        max_seconds=max_seconds,
        output_path=arguments.output,
        confidence=confidence,
        image_size=image_size,
        device=device,
        class_names=class_names,
    )
    _print_json(report)
    return 0
