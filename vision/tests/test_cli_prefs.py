from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.cli import _outline_style, build_parser, main
from cs2_vision_access.prefs import (
    OutlinePreferences,
    load_outline_preferences,
    save_outline_preferences,
)
from cs2_vision_access.renderer import get_outline_preset


class CliPrefsTests(unittest.TestCase):
    def test_prefs_file_overrides_preset_and_cli_overrides_prefs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"
            save_outline_preferences(
                prefs_path,
                OutlinePreferences(
                    preset="cyan-black",
                    fill_opacity=0.2,
                    stroke_pattern="dotted",
                    dash_period_px=20,
                ),
            )

            prefs_only = build_parser().parse_args(
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
                    "--prefs",
                    str(prefs_path),
                ]
            )
            style = _outline_style(prefs_only)
            cyan = get_outline_preset("cyan-black")
            self.assertEqual(style.inner_color, cyan.inner_color)
            self.assertEqual(style.outer_color, cyan.outer_color)
            self.assertEqual(style.fill_opacity, 0.2)
            self.assertEqual(style.stroke_pattern, "dotted")
            self.assertEqual(style.dash_period_px, 20)

            cli_override = build_parser().parse_args(
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
                    "--prefs",
                    str(prefs_path),
                    "--outline-preset",
                    "maximum-visibility",
                    "--fill-opacity",
                    "0.05",
                    "--stroke-pattern",
                    "dashed",
                    "--dash-period",
                    "8",
                    "--inner-color",
                    "#FFFFFF",
                    "--outer-color",
                    "#000000",
                ]
            )
            resolved = _outline_style(cli_override)
            maximum = get_outline_preset("maximum-visibility")
            self.assertEqual(resolved.inner_color, "#FFFFFF")
            self.assertEqual(resolved.outer_color, "#000000")
            self.assertEqual(resolved.fill_opacity, 0.05)
            self.assertEqual(resolved.stroke_pattern, "dashed")
            self.assertEqual(resolved.dash_period_px, 8)
            self.assertEqual(resolved.inner_width, maximum.inner_width)
            self.assertEqual(resolved.outer_width, maximum.outer_width)

    def test_fixed_widths_cli_overrides_prefs_scale(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"
            save_outline_preferences(
                prefs_path,
                OutlinePreferences(preset="high-visibility", scale_with_frame=True),
            )
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
                    "--prefs",
                    str(prefs_path),
                    "--fixed-widths",
                ]
            )
            self.assertFalse(_outline_style(arguments).scale_with_frame)

    def test_low_contrast_prefs_fail_before_model_loading(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "bad-prefs.json"
            prefs_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "preset": "high-visibility",
                        "inner_color": "#777777",
                        "outer_color": "#777777",
                    }
                ),
                encoding="utf-8",
            )
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
                        "--output",
                        "outlined.mp4",
                        "--prefs",
                        str(prefs_path),
                    ]
                )
            self.assertEqual(status, 2)
            self.assertIn("contrast ratio", stderr.getvalue())

    def test_prefs_show_set_reset_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"

            set_out = io.StringIO()
            with redirect_stdout(set_out):
                status = main(
                    [
                        "prefs",
                        "set",
                        "--path",
                        str(prefs_path),
                        "--outline-preset",
                        "maximum-visibility",
                        "--stroke-pattern",
                        "dashed",
                        "--dash-period",
                        "14",
                    ]
                )
            self.assertEqual(status, 0)
            set_payload = json.loads(set_out.getvalue())
            self.assertEqual(set_payload["preferences"]["preset"], "maximum-visibility")
            self.assertEqual(set_payload["preferences"]["stroke_pattern"], "dashed")
            self.assertEqual(set_payload["preferences"]["dash_period_px"], 14)
            self.assertTrue(prefs_path.is_file())

            show_out = io.StringIO()
            with redirect_stdout(show_out):
                status = main(["prefs", "show", "--path", str(prefs_path)])
            self.assertEqual(status, 0)
            show_payload = json.loads(show_out.getvalue())
            self.assertEqual(show_payload["preferences"]["preset"], "maximum-visibility")
            self.assertEqual(show_payload["resolved_style"]["stroke_pattern"], "dashed")
            self.assertGreaterEqual(show_payload["stroke_contrast_ratio"], 3.0)

            reset_out = io.StringIO()
            with redirect_stdout(reset_out):
                status = main(
                    [
                        "prefs",
                        "reset",
                        "--path",
                        str(prefs_path),
                        "--overwrite",
                    ]
                )
            self.assertEqual(status, 0)
            reset_payload = json.loads(reset_out.getvalue())
            self.assertEqual(reset_payload["preferences"]["preset"], "high-visibility")
            self.assertIsNone(reset_payload["preferences"]["stroke_pattern"])
            self.assertEqual(
                load_outline_preferences(prefs_path).resolve(),
                get_outline_preset("high-visibility"),
            )

    def test_prefs_set_preset_alone_is_sufficient(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "prefs",
                        "set",
                        "--path",
                        str(prefs_path),
                        "--outline-preset",
                        "cyan-black",
                    ]
                )
            self.assertEqual(status, 0)
            payload = json.loads(output.getvalue())
            self.assertEqual(payload["preferences"]["preset"], "cyan-black")
            self.assertIsNone(payload["preferences"]["inner_color"])
            cyan = get_outline_preset("cyan-black")
            self.assertEqual(payload["resolved_style"]["inner_color"], cyan.inner_color)

    def test_prefs_set_requires_overwrite_when_file_exists(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"
            with redirect_stdout(io.StringIO()):
                self.assertEqual(
                    main(
                        [
                            "prefs",
                            "set",
                            "--path",
                            str(prefs_path),
                            "--outline-preset",
                            "high-visibility",
                        ]
                    ),
                    0,
                )
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                status = main(
                    [
                        "prefs",
                        "set",
                        "--path",
                        str(prefs_path),
                        "--outline-preset",
                        "cyan-black",
                    ]
                )
            self.assertEqual(status, 2)
            self.assertIn("already exist", stderr.getvalue())

    def test_prefs_set_low_contrast_fails_before_write(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            prefs_path = Path(temporary_directory) / "outline-prefs.json"
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                status = main(
                    [
                        "prefs",
                        "set",
                        "--path",
                        str(prefs_path),
                        "--inner-color",
                        "#777777",
                        "--outer-color",
                        "#777777",
                    ]
                )
            self.assertEqual(status, 2)
            self.assertIn("contrast ratio", stderr.getvalue())
            self.assertFalse(prefs_path.exists())
