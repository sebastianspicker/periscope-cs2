"""Tests for the desktop GUI: settings model, pipeline controller, and CLI wiring.

No display or capture hardware is required; every hardware/dependency touch
point is mocked.
"""

from __future__ import annotations

import sys
import threading
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from cs2_vision_access.capture import CaptureConfig, LiveControlState
from cs2_vision_access.cli.parser import build_parser
from cs2_vision_access.config import (
    AppConfig,
    DisplayConfig,
    InputConfig,
    ModelConfig,
    OutlineConfig,
)
from cs2_vision_access.gui.controller import (
    GuiPipelineController,
    build_live_pipeline_config,
)
from cs2_vision_access.gui.model import GuiSettings
from cs2_vision_access.inference.pipeline import LivePipelineConfig, LiveRunSummary


def _summary() -> LiveRunSummary:
    return LiveRunSummary(
        frames_processed=100,
        frames_inferred=90,
        frames_dropped=10,
        instances_predicted=45,
        instances_outlined=40,
        temporal_suppressed=3,
        inference_ms_p50=12.5,
        inference_ms_p95=25.0,
        pipeline_ms_p50=15.0,
        pipeline_ms_p95=30.0,
        elapsed_seconds=10.0,
        capture_fps=60.0,
        throughput_fps=10.0,
        termination_reason="user_stop",
    )


def _distinct_config() -> AppConfig:
    return AppConfig(
        input=InputConfig(
            source_type="screen",
            device_index=3,
            monitor_index=2,
            region=(100, 200, 1280, 720),
            file_path="/videos/demo.mp4",
            width=1280,
            height=720,
            fps=30.0,
            backend="dshow",
        ),
        model=ModelConfig(
            path="models/custom.onnx",
            manifest="models/custom.json",
            backend="rfdetr",
            class_names=("player", "bot"),
            confidence=0.33,
            image_size=512,
            device="cuda:0",
        ),
        outline=OutlineConfig(
            preset="cyan-black",
            inner_color="#00FF00",
            outer_color="#000000",
            inner_width=4,
            outer_width=9,
            fill_opacity=0.12,
            stroke_pattern="dashed",
            output_mode="alpha",
            alpha_fill=True,
            temporal_enabled=True,
            temporal_min_frames=3,
        ),
        display=DisplayConfig(
            scale=0.75,
            headless=True,
            window_title="Test Overlay",
            overlay=True,
            overlay_x=42,
            overlay_y=24,
            overlay_monitor=1,
        ),
    )


class GuiSettingsRoundTripTests(unittest.TestCase):
    """GuiSettings <-> AppConfig round-trip must preserve key fields."""

    def test_from_config_preserves_distinct_values(self) -> None:
        settings = GuiSettings.from_config(_distinct_config())
        self.assertEqual(settings.class_names, ("player", "bot"))
        self.assertEqual(settings.region, (100, 200, 1280, 720))
        self.assertEqual(settings.overlay_x, 42)
        self.assertEqual(settings.overlay_y, 24)
        self.assertEqual(settings.overlay_monitor, 1)
        self.assertEqual(settings.output_mode, "alpha")

    def test_to_config_round_trip_preserves_key_fields(self) -> None:
        rebuilt = GuiSettings.from_config(_distinct_config()).to_config()
        self.assertEqual(rebuilt.model.class_names, ("player", "bot"))
        self.assertEqual(rebuilt.input.region, (100, 200, 1280, 720))
        self.assertEqual(rebuilt.input.source_type, "screen")
        self.assertEqual(rebuilt.model.backend, "rfdetr")
        self.assertEqual(rebuilt.outline.preset, "cyan-black")
        self.assertEqual(rebuilt.outline.output_mode, "alpha")
        self.assertEqual(getattr(rebuilt.display, "overlay_x", 0), 42)
        self.assertEqual(getattr(rebuilt.display, "overlay_y", 0), 24)
        self.assertEqual(getattr(rebuilt.display, "overlay_monitor", 0), 1)


class BuildPipelineConfigTests(unittest.TestCase):
    """build_live_pipeline_config must wire sources, overlay, and controls."""

    def test_screen_source_with_overlay(self) -> None:
        settings = GuiSettings(
            source_type="screen",
            monitor_index=1,
            output_mode="alpha",
            alpha_fill=True,
            temporal_enabled=True,
            temporal_min_frames=3,
        )
        control = LiveControlState(output_mode="alpha", alpha_fill=True, temporal_enabled=True)
        terminate = threading.Event()
        overlay = MagicMock()
        with patch("cs2_vision_access.gui.controller.ScreenCapturer") as screen_cap:
            config = build_live_pipeline_config(settings, control, terminate, overlay)
        screen_cap.assert_called_once_with(monitor_index=1)
        self.assertIs(config.overlay_window, overlay)
        self.assertIs(config.control_state, control)
        self.assertIs(config.terminate_event, terminate)
        self.assertTrue(config.headless)

    def test_capture_device_source_builds_capture_config(self) -> None:
        settings = GuiSettings(source_type="capture-device", device_index=2)
        config = build_live_pipeline_config(settings, LiveControlState(), threading.Event(), None)
        self.assertIsInstance(config.capture, CaptureConfig)
        assert isinstance(config.capture, CaptureConfig)
        self.assertEqual(config.capture.source, 2)


class ApplyActionTests(unittest.TestCase):
    """Named control actions must map onto the live control state."""

    def test_quit_action_returns_true(self) -> None:
        controller = GuiPipelineController(GuiSettings())
        self.assertTrue(controller.apply_action("quit"))

    def test_preset_two_sets_maximum_visibility(self) -> None:
        controller = GuiPipelineController(GuiSettings())
        controller.apply_action("preset-2")
        self.assertEqual(controller.control_state.current_preset, "maximum-visibility")


class PipelineLifecycleTests(unittest.TestCase):
    """start/stop must drive a worker thread, summary, and overlay cleanup."""

    def test_start_stop_runs_thread_and_stores_summary(self) -> None:
        summary = _summary()
        overlay = MagicMock()

        def fake_run(**kwargs: object) -> LiveRunSummary:
            config = kwargs["config"]
            assert isinstance(config, LivePipelineConfig)
            if config.terminate_event is not None:
                config.terminate_event.wait(timeout=0.5)
            return summary

        with (
            patch("cs2_vision_access.gui.controller.run_live_pipeline", side_effect=fake_run),
            patch("cs2_vision_access.gui.controller.is_overlay_available", return_value=True),
            patch("cs2_vision_access.gui.controller.create_overlay", return_value=overlay),
            patch("cs2_vision_access.gui.controller.ScreenCapturer"),
            patch("cs2_vision_access.gui.controller.build_segmenter", return_value=MagicMock()),
        ):
            controller = GuiPipelineController(GuiSettings(overlay=True))
            controller.start(on_frame=lambda *_: None, on_log=lambda _m: None)
            self.assertTrue(controller.is_running)
            controller.stop()
            self.assertFalse(controller.is_running)
            self.assertIs(controller.last_summary, summary)
            overlay.close.assert_called_once()


class FormatSummaryTests(unittest.TestCase):
    """format_summary must expose the key run numbers."""

    def test_format_summary_contains_key_numbers(self) -> None:
        controller = GuiPipelineController(GuiSettings())
        text = controller.format_summary(_summary())
        self.assertIn("100", text)
        self.assertIn("12.5", text)
        self.assertIn("10.0", text)
        self.assertIn("user_stop", text)


class GuiCliTests(unittest.TestCase):
    """The gui CLI command must register and stay display-import-safe."""

    def test_gui_command_registered_with_handler(self) -> None:
        arguments = build_parser().parse_args(["gui"])
        self.assertTrue(callable(getattr(arguments, "handler", None)))

    def test_gui_command_accepts_config_path(self) -> None:
        arguments = build_parser().parse_args(["gui", "--config", "x.json"])
        self.assertEqual(arguments.config, Path("x.json"))

    def test_handler_function_exists_and_is_callable(self) -> None:
        from cs2_vision_access.cli.handlers.gui import _handle_gui

        self.assertTrue(callable(_handle_gui))

    def test_gui_import_does_not_pull_in_tkinter(self) -> None:
        had_tkinter = "tkinter" in sys.modules
        import cs2_vision_access.cli.handlers.gui  # noqa: F401
        import cs2_vision_access.gui  # noqa: F401
        import cs2_vision_access.gui.app  # noqa: F401

        if not had_tkinter:
            self.assertNotIn("tkinter", sys.modules)


if __name__ == "__main__":
    unittest.main()
