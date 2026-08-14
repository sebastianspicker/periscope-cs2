"""outline, benchmark, and outline-presets subcommands."""

from __future__ import annotations

import argparse
from dataclasses import asdict
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.cli.inference_args import (
    _add_inference_arguments,
    _segmenter,
)
from cs2_vision_access.cli.style_args import _add_style_arguments, _outline_style
from cs2_vision_access.inference.temporal import TemporalStabilityConfig
from cs2_vision_access.prefs import load_outline_preferences
from cs2_vision_access.renderer import (
    OUTLINE_PRESET_DESCRIPTIONS,
    OUTLINE_PRESETS,
    OutlineRenderer,
    TreatmentCatalog,
    contrast_ratio,
    load_treatment_catalog,
)
from cs2_vision_access.video import process_video


def _temporal_config(arguments: argparse.Namespace) -> TemporalStabilityConfig:
    """Build suppress-only temporal config from CLI flags (off by default)."""
    enabled = bool(getattr(arguments, "temporal_suppress", False))
    min_frames = getattr(arguments, "temporal_min_frames", 2)
    if isinstance(min_frames, bool) or not isinstance(min_frames, int) or min_frames < 1:
        raise ValueError("--temporal-min-frames must be an integer >= 1")
    return TemporalStabilityConfig(
        enabled=enabled,
        min_consecutive_frames=min_frames,
    )


def register_outline_commands(subcommands: argparse._SubParsersAction) -> None:
    outline = subcommands.add_parser(
        "outline", help="render visible mask outlines into a local recorded video"
    )
    _add_inference_arguments(outline)
    outline.add_argument(
        "--output",
        type=Path,
        help="write an MP4 through an atomic partial file",
    )
    outline.add_argument(
        "--show",
        action="store_true",
        help="show the rendered local recording; q or Escape stops",
    )
    outline.add_argument(
        "--realtime-playback",
        action="store_true",
        help="pace recorded frames to the source FPS after processing",
    )
    outline.add_argument(
        "--overwrite",
        action="store_true",
        help=(
            "replace an existing MP4 output and/or cue log only after a "
            "successful run (atomic partial + replace)"
        ),
    )
    outline.add_argument(
        "--cue-log",
        type=Path,
        metavar="PATH",
        help=(
            "write offline instance enter/leave cue events as local JSONL "
            "(atomic partial; committed only on success)"
        ),
    )
    outline.add_argument(
        "--preview-frame",
        type=int,
        metavar="N",
        help=(
            "render only zero-based frame N as a single PNG (and/or --show) "
            "for A/B style checks without a full encode; use --max-frames / "
            "--max-seconds for short MP4 segments"
        ),
    )
    outline.add_argument(
        "--temporal-suppress",
        action="store_true",
        help=(
            "opt-in suppress-only anti-flash: drop detections until they appear "
            "on min consecutive frames; never hold last mask (safe default off)"
        ),
    )
    outline.add_argument(
        "--temporal-min-frames",
        type=int,
        default=2,
        metavar="N",
        help=(
            "with --temporal-suppress, require N consecutive matched frames "
            "before drawing (default: 2)"
        ),
    )
    _add_style_arguments(outline, include_role_config=True)
    outline.set_defaults(handler=_handle_outline)

    benchmark = subcommands.add_parser(
        "benchmark", help="measure the file-only inference path without saving frames"
    )
    _add_inference_arguments(benchmark)
    benchmark.add_argument(
        "--cue-log",
        type=Path,
        metavar="PATH",
        help=(
            "write offline instance enter/leave cue events as local JSONL "
            "(atomic partial; committed only on success)"
        ),
    )
    benchmark.add_argument(
        "--overwrite",
        action="store_true",
        help=("replace an existing cue log only after a successful run (atomic partial + replace)"),
    )
    benchmark.add_argument(
        "--temporal-suppress",
        action="store_true",
        help=(
            "opt-in suppress-only anti-flash: drop detections until they appear "
            "on min consecutive frames; never hold last mask (safe default off)"
        ),
    )
    benchmark.add_argument(
        "--temporal-min-frames",
        type=int,
        default=2,
        metavar="N",
        help=(
            "with --temporal-suppress, require N consecutive matched frames "
            "before drawing (default: 2)"
        ),
    )
    _add_style_arguments(benchmark, include_role_config=True)
    benchmark.set_defaults(handler=_handle_benchmark)

    presets = subcommands.add_parser(
        "outline-presets",
        help="list one-command accessibility outline treatments as JSON",
    )
    _add_style_arguments(presets)
    presets.set_defaults(handler=_handle_outline_presets)


def _handle_outline(arguments: argparse.Namespace) -> int:
    preview_frame = getattr(arguments, "preview_frame", None)
    if preview_frame is not None:
        if (
            isinstance(preview_frame, bool)
            or not isinstance(preview_frame, int)
            or preview_frame < 0
        ):
            raise ValueError("--preview-frame must be a non-negative integer")
        if arguments.output is None and not arguments.show:
            raise ValueError("outline --preview-frame requires --output (.png), --show, or both")
        if arguments.output is not None and arguments.output.suffix.lower() != ".png":
            raise ValueError(
                "outline --preview-frame output must use the .png extension "
                "(use --max-frames / --max-seconds for short MP4 segments)"
            )
    else:
        if arguments.output is None and not arguments.show:
            raise ValueError("outline requires --output, --show, or both")
        if arguments.output is not None and arguments.output.suffix.lower() == ".png":
            raise ValueError(
                "PNG output requires --preview-frame N "
                "(full encode uses .mp4; short segments use --max-frames / --max-seconds)"
            )
    # Resolve style (contrast, geometry, pattern) before any model I/O.
    style = _outline_style(arguments)
    catalog = _role_catalog(arguments)
    summary = process_video(
        input_path=arguments.input,
        segmenter=_segmenter(arguments),
        renderer=OutlineRenderer(style, catalog=catalog),
        output_path=arguments.output,
        display=arguments.show,
        realtime_playback=arguments.realtime_playback,
        max_frames=arguments.max_frames,
        overwrite=arguments.overwrite,
        max_seconds=arguments.max_seconds,
        cue_log_path=arguments.cue_log,
        preview_frame=preview_frame,
        temporal_config=_temporal_config(arguments),
    )
    _print_json(summary.as_dict())
    return 0


def _handle_benchmark(arguments: argparse.Namespace) -> int:
    style = _outline_style(arguments)
    catalog = _role_catalog(arguments)
    summary = process_video(
        input_path=arguments.input,
        segmenter=_segmenter(arguments),
        renderer=OutlineRenderer(style, catalog=catalog),
        output_path=None,
        display=False,
        realtime_playback=False,
        max_frames=arguments.max_frames,
        overwrite=arguments.overwrite,
        max_seconds=arguments.max_seconds,
        cue_log_path=arguments.cue_log,
        temporal_config=_temporal_config(arguments),
    )
    _print_json(summary.as_dict())
    return 0


def _role_catalog(arguments: argparse.Namespace) -> TreatmentCatalog | None:
    """Load optional multi-signifier role catalog; fail closed before model I/O."""
    path = getattr(arguments, "role_config", None)
    if path is None:
        return None
    return load_treatment_catalog(path)


def _handle_outline_presets(arguments: argparse.Namespace) -> int:
    presets: dict[str, object] = {}
    for name, style in sorted(OUTLINE_PRESETS.items()):
        presets[name] = {
            "description": OUTLINE_PRESET_DESCRIPTIONS[name],
            "style": asdict(style),
            "stroke_contrast_ratio": round(
                contrast_ratio(style.inner_color, style.outer_color),
                2,
            ),
        }
    payload: dict[str, object] = {"presets": presets}
    if (
        arguments.prefs is not None
        or any(
            value is not None
            for value in (
                arguments.outline_preset,
                arguments.inner_color,
                arguments.outer_color,
                arguments.inner_width,
                arguments.outer_width,
                arguments.fill_opacity,
                arguments.stroke_pattern,
                arguments.dash_period,
                arguments.fill_mode,
                arguments.halo_blur,
                arguments.outline_kernel,
            )
        )
        or arguments.fixed_widths
        or arguments.adapt_width
    ):
        style = _outline_style(arguments)
        payload["resolved_style"] = asdict(style)
        payload["stroke_contrast_ratio"] = round(
            contrast_ratio(style.inner_color, style.outer_color),
            2,
        )
        if arguments.prefs is not None:
            payload["preferences"] = load_outline_preferences(arguments.prefs).as_json()
    _print_json(payload)
    return 0
