"""Tests for the live command registration and CLI flags."""

from __future__ import annotations

import unittest


class TestLiveCommandInCLI(unittest.TestCase):
    """Verify the live command is registered in the CLI parser."""

    def test_live_command_is_registered(self) -> None:
        import argparse

        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        subcommands = {
            action.dest: action.choices
            for action in parser._actions
            if isinstance(action, argparse._SubParsersAction)
        }
        all_commands: set[str] = set()
        for choices in subcommands.values():
            if choices:
                all_commands.update(choices)
        self.assertIn("live", all_commands)

    def test_live_help_output(self) -> None:
        import argparse

        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        # argparse exits with SystemExit(0) on --help; assert the text instead.
        self.assertIn("live", parser.format_help())
        live_parser = None
        for action in parser._actions:
            if isinstance(action, argparse._SubParsersAction) and "live" in action.choices:
                live_parser = action.choices["live"]
                break
        self.assertIsNotNone(live_parser)
        assert live_parser is not None
        self.assertIn("--list-devices", live_parser.format_help())

    def test_live_list_devices_flag(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(["live", "--list-devices"])
        self.assertTrue(args.list_devices)

    def test_live_default_input_device(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(
            [
                "live",
                "--model",
                "test.onnx",
                "--manifest",
                "test.model.json",
            ]
        )
        self.assertIsNone(args.input_device)

    def test_live_custom_input_device(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(
            [
                "live",
                "--input-device",
                "2",
                "--model",
                "test.onnx",
                "--manifest",
                "test.model.json",
            ]
        )
        self.assertEqual(args.input_device, "2")

    def test_live_output_mode_default(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(
            [
                "live",
                "--model",
                "test.onnx",
                "--manifest",
                "test.model.json",
            ]
        )
        self.assertIsNone(args.output_mode)

    def test_live_alpha_only_flag(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(
            [
                "live",
                "--model",
                "test.onnx",
                "--manifest",
                "test.model.json",
                "--alpha-only",
            ]
        )
        self.assertTrue(args.alpha_only)

    def test_live_headless_flag(self) -> None:
        from cs2_vision_access.cli.parser import build_parser

        parser = build_parser()
        args = parser.parse_args(
            [
                "live",
                "--model",
                "test.onnx",
                "--manifest",
                "test.model.json",
                "--headless",
            ]
        )
        self.assertTrue(args.headless)


if __name__ == "__main__":
    unittest.main()
