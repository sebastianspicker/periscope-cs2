"""Tests for overlay-position config/CLI wiring and the download-model manifest fix.

Covers:
* ``download-model`` ONNX-direct handling when the registry entry has no classes
* ``DisplayConfig`` overlay position fields and config JSON round-trip
* ``live`` parser options for ``--overlay-x`` / ``--overlay-y`` / ``--overlay-monitor``
* overlay origin resolution (explicit position and screen auto-align)
* forbidden-token compliance for the full CLI parser
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import MagicMock, patch

from cs2_vision_access.cli import main
from cs2_vision_access.cli.handlers import download_model as download_model_module
from cs2_vision_access.cli.handlers.download_model import _handle_onnx_direct
from cs2_vision_access.cli.parser import build_parser
from cs2_vision_access.config import (
    AppConfig,
    DisplayConfig,
    InputConfig,
    load_config,
    save_config,
)

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


def _is_forbidden_cli_token(name: str) -> bool:
    """True when a CLI command or option is a forbidden bare name."""
    return name.casefold().lstrip("-") in _FORBIDDEN_CLI_BARE_NAMES


class TestDownloadModelNoManifest(unittest.TestCase):
    """The no-classes ONNX-direct path must skip the manifest cleanly."""

    def setUp(self) -> None:
        self.temp_dir = Path(tempfile.mkdtemp())
        self.addCleanup(self._cleanup)

    def _cleanup(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    _MODEL_BYTES = b"fake onnx bytes\n"
    _MODEL_SHA256 = hashlib.sha256(_MODEL_BYTES).hexdigest()

    @classmethod
    def _fake_download(cls, url: str, path: object, *, expected_sha256: str) -> str:
        Path(str(path)).write_bytes(cls._MODEL_BYTES)
        return expected_sha256

    def test_onnx_direct_without_classes_skips_manifest(self) -> None:
        arguments = argparse.Namespace(
            model_name="edge-test",
            output_dir=self.temp_dir,
            image_size=1024,
            device="cpu",
            overwrite=False,
            list_models=False,
            classes=None,
            origin=None,
            license=None,
        )
        registry_info = {
            "format": "onnx",
            "url": "https://example.com/edge_sam_3x_encoder.onnx",
            "sha256": self._MODEL_SHA256,
            "task": "segment",
            "imgsz": 1024,
            "license": "MIT",
        }

        buf = io.StringIO()
        with (
            patch.object(
                download_model_module,
                "_download_https",
                side_effect=self._fake_download,
            ),
            redirect_stdout(buf),
        ):
            status = _handle_onnx_direct("edge-test", registry_info, arguments)

        self.assertEqual(status, 0)
        out = buf.getvalue()
        self.assertIn("no manifest", out)
        self.assertNotIn("--manifest .", out)
        self.assertNotIn("--manifest ", out)
        self.assertNotIn("\n  .  (", out)
        self.assertIn(str(self.temp_dir.resolve()), out)

    def test_main_onnx_direct_without_classes(self) -> None:
        registry = {
            "edge-test": {
                "description": "test entry without class metadata",
                "format": "onnx",
                "url": "https://example.com/edge_test.onnx",
                "sha256": self._MODEL_SHA256,
                "task": "segment",
                "imgsz": 1024,
                "license": "MIT",
            }
        }

        buf = io.StringIO()
        with (
            patch.object(download_model_module, "MODEL_REGISTRY", registry),
            patch.object(
                download_model_module,
                "_download_https",
                side_effect=self._fake_download,
            ),
            redirect_stdout(buf),
        ):
            status = main(
                [
                    "download-model",
                    "edge-test",
                    "--output-dir",
                    str(self.temp_dir),
                ]
            )

        self.assertEqual(status, 0)
        out = buf.getvalue()
        self.assertIn("no manifest", out)
        self.assertNotIn("--manifest .", out)
        self.assertNotIn("\n  .  (", out)


class TestOverlayConfigFields(unittest.TestCase):
    """DisplayConfig overlay position fields and JSON round-trip."""

    def test_display_config_defaults_include_overlay_position(self) -> None:
        cfg = DisplayConfig()
        self.assertEqual(cfg.overlay_x, 0)
        self.assertEqual(cfg.overlay_y, 0)
        self.assertEqual(cfg.overlay_monitor, 0)

    def test_save_load_round_trip_preserves_overlay_position(self) -> None:
        config = AppConfig(
            display=DisplayConfig(
                scale=0.5,
                headless=False,
                window_title="Test Overlay",
                overlay=True,
                overlay_x=320,
                overlay_y=240,
                overlay_monitor=2,
            )
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            save_config(config, path=path, overwrite=True)
            loaded = load_config(path)

        self.assertEqual(loaded.display.overlay_x, 320)
        self.assertEqual(loaded.display.overlay_y, 240)
        self.assertEqual(loaded.display.overlay_monitor, 2)

    def test_load_config_json_with_overlay_keys(self) -> None:
        payload = {
            "schema_version": 1,
            "display": {
                "scale": 0.5,
                "headless": False,
                "window_title": "CS2 Vision Access - Ingame Overlay",
                "overlay": True,
                "overlay_x": 100,
                "overlay_y": 50,
                "overlay_monitor": 1,
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            loaded = load_config(path)

        self.assertEqual(loaded.display.overlay_x, 100)
        self.assertEqual(loaded.display.overlay_y, 50)
        self.assertEqual(loaded.display.overlay_monitor, 1)


class TestLiveOverlayCliOptions(unittest.TestCase):
    """The live subcommand parses the overlay position options."""

    def test_live_overlay_position_options_parse(self) -> None:
        arguments = build_parser().parse_args(
            [
                "live",
                "--overlay",
                "--overlay-x",
                "100",
                "--overlay-y",
                "50",
                "--overlay-monitor",
                "2",
                "--model",
                "m.onnx",
                "--manifest",
                "m.json",
            ]
        )
        self.assertTrue(arguments.overlay)
        self.assertEqual(arguments.overlay_x, 100)
        self.assertEqual(arguments.overlay_y, 50)
        self.assertEqual(arguments.overlay_monitor, 2)

    def test_live_overlay_position_defaults(self) -> None:
        arguments = build_parser().parse_args(
            [
                "live",
                "--model",
                "m.onnx",
                "--manifest",
                "m.json",
            ]
        )
        self.assertEqual(arguments.overlay_x, 0)
        self.assertEqual(arguments.overlay_y, 0)
        self.assertEqual(arguments.overlay_monitor, 0)


class TestBuildOverlay(unittest.TestCase):
    """Overlay window origin resolution in ``_build_overlay``."""

    def test_explicit_overlay_position_is_forwarded(self) -> None:
        from cs2_vision_access.cli.handlers.live import _build_overlay

        arguments = argparse.Namespace(
            overlay=True,
            overlay_x=100,
            overlay_y=50,
            overlay_monitor=0,
            monitor=None,
            source_type="capture-device",
        )
        display_config = DisplayConfig(overlay=True)
        mock_overlay = MagicMock()
        with (
            patch(
                "cs2_vision_access.cli.handlers.live.is_overlay_available",
                return_value=True,
            ),
            patch(
                "cs2_vision_access.cli.handlers.live.create_overlay",
                return_value=mock_overlay,
            ) as mock_create,
        ):
            overlay = _build_overlay(
                arguments,
                display_config,
                headless=False,
                width=1920,
                height=1080,
            )

        self.assertIs(overlay, mock_overlay)
        mock_create.assert_called_once_with(
            title=display_config.window_title,
            width=1920,
            height=1080,
            x=100,
            y=50,
        )

    def test_screen_source_auto_aligns_to_capture_monitor(self) -> None:
        from cs2_vision_access.cli.handlers.live import _build_overlay

        arguments = argparse.Namespace(
            overlay=True,
            overlay_x=0,
            overlay_y=0,
            overlay_monitor=0,
            monitor=2,
            source_type="screen",
        )
        display_config = DisplayConfig(overlay=True)
        mock_overlay = MagicMock()
        monitors = [
            {"index": 1, "left": 0, "top": 0, "width": 1920, "height": 1080, "name": "Monitor 1"},
            {
                "index": 2,
                "left": 1920,
                "top": 0,
                "width": 1920,
                "height": 1080,
                "name": "Monitor 2",
            },
        ]
        with (
            patch(
                "cs2_vision_access.cli.handlers.live.is_overlay_available",
                return_value=True,
            ),
            patch(
                "cs2_vision_access.cli.handlers.live.create_overlay",
                return_value=mock_overlay,
            ) as mock_create,
            patch(
                "cs2_vision_access.cli.handlers.live.list_monitors",
                return_value=monitors,
            ),
        ):
            overlay = _build_overlay(
                arguments,
                display_config,
                headless=False,
                width=1920,
                height=1080,
            )

        self.assertIs(overlay, mock_overlay)
        mock_create.assert_called_once_with(
            title=display_config.window_title,
            width=1920,
            height=1080,
            x=1920,
            y=0,
        )

    def test_screen_region_auto_aligns_to_region_origin(self) -> None:
        from cs2_vision_access.cli.handlers.live import _build_overlay

        arguments = argparse.Namespace(
            overlay=True,
            overlay_x=0,
            overlay_y=0,
            overlay_monitor=0,
            monitor=None,
            source_type="screen",
        )
        display_config = DisplayConfig(overlay=True)
        input_config = InputConfig(source_type="screen", region=(100, 200, 640, 480))
        mock_overlay = MagicMock()
        with (
            patch(
                "cs2_vision_access.cli.handlers.live.is_overlay_available",
                return_value=True,
            ),
            patch(
                "cs2_vision_access.cli.handlers.live.create_overlay",
                return_value=mock_overlay,
            ) as mock_create,
        ):
            overlay = _build_overlay(
                arguments,
                display_config,
                headless=False,
                width=640,
                height=480,
                source_type="screen",
                input_config=input_config,
            )

        self.assertIs(overlay, mock_overlay)
        mock_create.assert_called_once_with(
            title=display_config.window_title,
            width=640,
            height=480,
            x=100,
            y=200,
        )

    def test_no_overlay_requested_returns_none(self) -> None:
        from cs2_vision_access.cli.handlers.live import _build_overlay

        arguments = argparse.Namespace(
            overlay=False,
            overlay_x=0,
            overlay_y=0,
            overlay_monitor=0,
            monitor=None,
            source_type="screen",
        )
        display_config = DisplayConfig(overlay=False)
        with patch("cs2_vision_access.cli.handlers.live.is_overlay_available", return_value=True):
            overlay = _build_overlay(
                arguments,
                display_config,
                headless=False,
                width=1920,
                height=1080,
            )
        self.assertIsNone(overlay)


class TestForbiddenCliTokens(unittest.TestCase):
    """No CLI command or option may use a forbidden bare name."""

    def test_parser_has_no_forbidden_option_names(self) -> None:
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
        findings = sorted(value for value in exposed if _is_forbidden_cli_token(value))
        self.assertEqual(findings, [])


if __name__ == "__main__":
    unittest.main()
