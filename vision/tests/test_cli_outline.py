from __future__ import annotations

import argparse
import io
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.cli import _outline_style, build_parser, main
from cs2_vision_access.renderer import OutlineStyle, get_outline_preset

_FORBIDDEN_CLI_BARE_NAMES: frozenset[str] = frozenset(
    {
        "aim",
        "fire",
        "hook",
        "inject",
        "memory",
        "mouse",
        "process",
    }
)


def is_forbidden_cli_token(name: str) -> bool:
    bare = name.casefold().lstrip("-")
    return bare in _FORBIDDEN_CLI_BARE_NAMES


class CliOutlineTests(unittest.TestCase):
    def test_release_parser_has_no_live_capture_command(self) -> None:
        parser = build_parser()
        command_names: set[str] = set()
        option_names: set[str] = set()
        pending = [parser]
        while pending:
            current = pending.pop()
            for action in current._actions:
                option_names.update(action.option_strings)
                if isinstance(action, argparse._SubParsersAction):
                    command_names.update(action.choices)
                    pending.extend(action.choices.values())

        exposed = command_names | option_names
        findings = sorted(value for value in exposed if is_forbidden_cli_token(value))

        self.assertEqual(findings, [])

    def test_outline_requires_output_or_show_before_model_loading(self) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            status = main(
                [
                    "outline",
                    "--input",
                    str(Path("missing.mp4")),
                    "--model",
                    str(Path("missing.onnx")),
                    "--manifest",
                    str(Path("missing.json")),
                ]
            )

        self.assertEqual(status, 2)
        self.assertIn("requires --output", stderr.getvalue())

    def test_outline_presets_are_machine_readable(self) -> None:
        output = io.StringIO()
        with redirect_stdout(output):
            status = main(["outline-presets"])

        self.assertEqual(status, 0)
        self.assertIn('"maximum-visibility"', output.getvalue())
        self.assertIn('"stroke_contrast_ratio"', output.getvalue())

    def test_low_contrast_custom_outline_fails_before_model_loading(self) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            status = main(
                [
                    "outline",
                    "--input",
                    "missing.mp4",
                    "--model",
                    "missing.onnx",
                    "--manifest",
                    "missing.json",
                    "--output",
                    "outlined.mp4",
                    "--inner-color",
                    "#777777",
                    "--outer-color",
                    "#777777",
                ]
            )

        self.assertEqual(status, 2)
        self.assertIn("contrast ratio", stderr.getvalue())

    def test_coco_classes_file_is_accepted_by_register_parser(self) -> None:
        arguments = build_parser().parse_args(
            [
                "register-model",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--classes-json",
                "src/cs2_vision_access/config/data/coco80.json",
                "--origin",
                "official",
                "--license",
                "AGPL-3.0-only",
            ]
        )

        self.assertEqual(
            arguments.classes_json,
            Path("src/cs2_vision_access/config/data/coco80.json"),
        )

    def test_outline_defaults_to_ultralytics_onnx_backend(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
            ]
        )

        self.assertEqual(arguments.backend, "ultralytics-onnx")

    def test_benchmark_accepts_explicit_backend(self) -> None:
        arguments = build_parser().parse_args(
            [
                "benchmark",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--backend",
                "ultralytics-onnx",
            ]
        )

        self.assertEqual(arguments.backend, "ultralytics-onnx")

    def test_backend_is_casefolded_by_parser(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--backend",
                "Ultralytics-ONNX",
            ]
        )

        self.assertEqual(arguments.backend, "ultralytics-onnx")

    def test_unknown_backend_is_rejected_at_parse_time(self) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
            build_parser().parse_args(
                [
                    "outline",
                    "--input",
                    "input.mp4",
                    "--model",
                    "model.onnx",
                    "--manifest",
                    "model.json",
                    "--output",
                    "outlined.mp4",
                    "--backend",
                    "not-a-backend",
                ]
            )

        self.assertNotEqual(raised.exception.code, 0)
        self.assertIn("unknown or unsupported", stderr.getvalue())

    def test_rfdetr_backend_alias_is_accepted_at_parse_time(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--backend",
                "RF-DETR",
            ]
        )

        self.assertEqual(arguments.backend, "rfdetr")

    def test_style_flags_include_stroke_pattern_and_dash_period(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--stroke-pattern",
                "dashed",
                "--dash-period",
                "16",
            ]
        )
        self.assertEqual(arguments.stroke_pattern, "dashed")
        self.assertEqual(arguments.dash_period, 16)
        style = _outline_style(arguments)
        self.assertEqual(style.stroke_pattern, "dashed")
        self.assertEqual(style.dash_period_px, 16)

    def test_outline_accepts_role_config_path(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--role-config",
                "docs/examples/role-catalog.v1.json",
            ]
        )
        self.assertEqual(
            arguments.role_config,
            Path("docs/examples/role-catalog.v1.json"),
        )

    def test_cue_log_flag_is_accepted_by_outline_and_benchmark(self) -> None:
        outline_args = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--cue-log",
                "cues.jsonl",
            ]
        )
        self.assertEqual(outline_args.cue_log, Path("cues.jsonl"))
        benchmark_args = build_parser().parse_args(
            [
                "benchmark",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--cue-log",
                "cues.jsonl",
            ]
        )
        self.assertEqual(benchmark_args.cue_log, Path("cues.jsonl"))

    def test_temporal_suppress_flags_default_off_and_parse(self) -> None:
        default_args = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
            ]
        )
        self.assertFalse(default_args.temporal_suppress)
        self.assertEqual(default_args.temporal_min_frames, 2)

        enabled_args = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
                "--temporal-suppress",
                "--temporal-min-frames",
                "3",
            ]
        )
        self.assertTrue(enabled_args.temporal_suppress)
        self.assertEqual(enabled_args.temporal_min_frames, 3)

        benchmark_args = build_parser().parse_args(
            [
                "benchmark",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--temporal-suppress",
            ]
        )
        self.assertTrue(benchmark_args.temporal_suppress)
        self.assertEqual(benchmark_args.temporal_min_frames, 2)

    def test_default_style_without_flags_matches_high_visibility(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--output",
                "outlined.mp4",
            ]
        )
        self.assertEqual(_outline_style(arguments), OutlineStyle())
        self.assertEqual(_outline_style(arguments), get_outline_preset("high-visibility"))

    def test_outline_preview_frame_requires_png_output(self) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            status = main(
                [
                    "outline",
                    "--input",
                    "missing.mp4",
                    "--model",
                    "missing.onnx",
                    "--manifest",
                    "missing.json",
                    "--preview-frame",
                    "3",
                    "--output",
                    "outlined.mp4",
                ]
            )
        self.assertEqual(status, 2)
        self.assertIn(".png", stderr.getvalue())

    def test_outline_png_without_preview_frame_is_rejected(self) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            status = main(
                [
                    "outline",
                    "--input",
                    "missing.mp4",
                    "--model",
                    "missing.onnx",
                    "--manifest",
                    "missing.json",
                    "--output",
                    "frame.png",
                ]
            )
        self.assertEqual(status, 2)
        self.assertIn("--preview-frame", stderr.getvalue())

    def test_outline_preview_frame_is_accepted_by_parser(self) -> None:
        arguments = build_parser().parse_args(
            [
                "outline",
                "--input",
                "input.mp4",
                "--model",
                "model.onnx",
                "--manifest",
                "model.json",
                "--preview-frame",
                "12",
                "--output",
                "preview.png",
                "--outline-preset",
                "maximum-visibility",
            ]
        )
        self.assertEqual(arguments.preview_frame, 12)
        self.assertEqual(arguments.output, Path("preview.png"))
        self.assertEqual(
            _outline_style(arguments),
            get_outline_preset("maximum-visibility"),
        )

    def test_outline_preview_frame_style_fails_before_model_loading(self) -> None:
        stderr = io.StringIO()
        with (
            redirect_stderr(stderr),
            patch(
                "cs2_vision_access.cli.inference_args.create_segmenter",
                side_effect=AssertionError("model must not load"),
            ),
        ):
            status = main(
                [
                    "outline",
                    "--input",
                    "missing.mp4",
                    "--model",
                    "missing.onnx",
                    "--manifest",
                    "missing.json",
                    "--preview-frame",
                    "0",
                    "--output",
                    "preview.png",
                    "--inner-color",
                    "#777777",
                    "--outer-color",
                    "#777777",
                ]
            )
        self.assertEqual(status, 2)
        self.assertIn("contrast ratio", stderr.getvalue())
