"""Tests for overlay positioning, hotkey actions, and pipeline termination wiring.

Covers the overlay facade/backends without constructing real windows: backends
are exercised through fakes, tkinter is stubbed out of ``sys.modules``, and the
pipeline uses the same cv2/capture mocking pattern as ``test_live.py``.
"""

from __future__ import annotations

import sys
import threading
import types
import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.capture.base import CaptureConfig
from cs2_vision_access.capture.hotkeys import (
    HOTKEY_ACTIONS,
    LiveControlState,
    apply_hotkey_action,
    handle_live_key,
)
from cs2_vision_access.capture.overlay import OverlayWindow
from cs2_vision_access.capture.overlay_backends.tkinter_backend import TkinterOverlayBackend
from cs2_vision_access.capture.overlay_backends.win32 import Win32OverlayBackend
from cs2_vision_access.inference.pipeline.config import LivePipelineConfig

_DEFAULT_TITLE = "CS2 Vision Access Overlay"


def _noop_printer(message: str) -> None:
    """Discard status output for tests that only assert on state."""
    del message


class _FakeBackend:
    """Minimal backend implementing the OverlayBackend protocol (no real window)."""

    def __init__(self) -> None:
        self.open_calls: list[tuple[int, int, str, int, int]] = []
        self.move_calls: list[tuple[int, int]] = []
        self.hotkey_handler: object = None
        self._position = (0, 0)
        self._open = False

    @property
    def is_open(self) -> bool:
        return self._open

    @property
    def position(self) -> tuple[int, int]:
        return self._position

    def open(
        self,
        width: int,
        height: int,
        title: str = _DEFAULT_TITLE,
        x: int = 0,
        y: int = 0,
    ) -> None:
        self.open_calls.append((width, height, title, x, y))
        self._position = (x, y)
        self._open = True

    def move(self, x: int, y: int) -> None:
        self.move_calls.append((x, y))
        self._position = (x, y)

    def close(self) -> None:
        self._open = False

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        del frame_rgba

    def poll_events(self) -> bool:
        return self._open

    def set_hotkey_handler(self, handler: object) -> None:
        self.hotkey_handler = handler


class TestOverlayWindowFacade(unittest.TestCase):
    """OverlayWindow forwards open/move/hotkey calls to the injected backend."""

    def test_open_forwards_position_to_backend(self) -> None:
        backend = _FakeBackend()
        overlay = OverlayWindow(backend=backend)
        overlay.open(1920, 1080, x=100, y=50)
        self.assertEqual(backend.open_calls, [(1920, 1080, _DEFAULT_TITLE, 100, 50)])
        self.assertEqual(overlay.position, (100, 50))
        self.assertEqual(backend.position, (100, 50))

    def test_move_forwards_and_updates_position(self) -> None:
        backend = _FakeBackend()
        overlay = OverlayWindow(backend=backend)
        overlay.open(1920, 1080, x=100, y=50)
        overlay.move(300, 200)
        self.assertEqual(backend.move_calls, [(300, 200)])
        self.assertEqual(overlay.position, (300, 200))
        self.assertEqual(backend.position, (300, 200))

    def test_set_hotkey_handler_forwards(self) -> None:
        backend = _FakeBackend()
        overlay = OverlayWindow(backend=backend)
        handler = MagicMock()
        overlay.set_hotkey_handler(handler)
        self.assertIs(backend.hotkey_handler, handler)


class TestApplyHotkeyAction(unittest.TestCase):
    """apply_hotkey_action mutates LiveControlState per named action."""

    def test_preset_1_sets_high_visibility(self) -> None:
        state = LiveControlState()
        self.assertFalse(apply_hotkey_action("preset-1", state, printer=_noop_printer))
        self.assertEqual(state.current_preset, "high-visibility")
        self.assertIsNotNone(state.style_override)

    def test_preset_2_sets_maximum_visibility(self) -> None:
        state = LiveControlState()
        apply_hotkey_action("preset-2", state, printer=_noop_printer)
        self.assertEqual(state.current_preset, "maximum-visibility")
        self.assertIsNotNone(state.style_override)

    def test_width_up_clamps_at_20(self) -> None:
        state = LiveControlState(width_offset=19)
        apply_hotkey_action("width-up", state, printer=_noop_printer)
        self.assertEqual(state.width_offset, 20)
        apply_hotkey_action("width-up", state, printer=_noop_printer)
        self.assertEqual(state.width_offset, 20)

    def test_width_down_clamps_at_minus_5(self) -> None:
        state = LiveControlState(width_offset=-5)
        apply_hotkey_action("width-down", state, printer=_noop_printer)
        self.assertEqual(state.width_offset, -5)

    def test_mode_cycle_order(self) -> None:
        state = LiveControlState(output_mode="overlay")
        apply_hotkey_action("mode-cycle", state, printer=_noop_printer)
        self.assertEqual(state.output_mode, "alpha")
        apply_hotkey_action("mode-cycle", state, printer=_noop_printer)
        self.assertEqual(state.output_mode, "green")
        apply_hotkey_action("mode-cycle", state, printer=_noop_printer)
        self.assertEqual(state.output_mode, "overlay")

    def test_pause_prints_status(self) -> None:
        captured: list[str] = []
        state = LiveControlState()
        apply_hotkey_action("pause", state, printer=captured.append)
        self.assertEqual(captured, ["[live] PAUSED"])
        apply_hotkey_action("pause", state, printer=captured.append)
        self.assertEqual(captured, ["[live] PAUSED", "[live] RESUMED"])

    def test_quit_returns_true(self) -> None:
        state = LiveControlState()
        self.assertTrue(apply_hotkey_action("quit", state, printer=_noop_printer))

    def test_unknown_action_returns_false_and_changes_nothing(self) -> None:
        state = LiveControlState(width_offset=2, current_preset="cyan-black")
        self.assertFalse(apply_hotkey_action("unknown-action", state, printer=_noop_printer))
        self.assertEqual(state.width_offset, 2)
        self.assertEqual(state.current_preset, "cyan-black")

    def test_hotkey_action_names(self) -> None:
        self.assertEqual(
            HOTKEY_ACTIONS,
            frozenset(
                {
                    "quit",
                    "pause",
                    "preset-1",
                    "preset-2",
                    "preset-3",
                    "width-up",
                    "width-down",
                    "temporal",
                    "mode-cycle",
                    "fill",
                    "diagnostics",
                }
            ),
        )


class TestHandleLiveKey(unittest.TestCase):
    """handle_live_key still maps cv2 key codes to the same behavior."""

    def test_preset_2_key_maps_to_maximum_visibility(self) -> None:
        state = LiveControlState()
        self.assertFalse(handle_live_key(ord("2"), state, cv2=object()))
        self.assertEqual(state.current_preset, "maximum-visibility")

    def test_plus_key_increments_width(self) -> None:
        state = LiveControlState()
        handle_live_key(ord("+"), state, cv2=object())
        self.assertEqual(state.width_offset, 1)

    def test_escape_key_requests_quit(self) -> None:
        state = LiveControlState()
        self.assertTrue(handle_live_key(27, state, cv2=object()))


class _FakeTclError(Exception):
    """Stand-in for tk.TclError when -transparentcolor is unsupported."""


class _FakeTkRoot:
    def __init__(self) -> None:
        self.geometry_calls: list[str] = []
        self.attributes_calls: list[tuple[object, ...]] = []

    def title(self, title: str) -> None:
        del title

    def overrideredirect(self, flag: bool) -> None:
        del flag

    def attributes(self, *args: object) -> object:
        self.attributes_calls.append(args)
        if args and args[0] == "-transparentcolor":
            raise _FakeTclError()
        return True

    def geometry(self, geometry: str) -> None:
        self.geometry_calls.append(geometry)

    def configure(self, **kwargs: object) -> None:
        del kwargs

    def destroy(self) -> None:
        pass

    def update(self) -> None:
        pass

    def update_idletasks(self) -> None:
        pass


class _FakeTkCanvas:
    def __init__(self, *args: object, **kwargs: object) -> None:
        del args, kwargs
        self.image_id = 1

    def pack(self) -> None:
        pass

    def create_image(self, *args: object, **kwargs: object) -> int:
        del args, kwargs
        return self.image_id

    def itemconfig(self, *args: object, **kwargs: object) -> None:
        del args, kwargs


class _FakeTkPhotoImage:
    def __init__(self, **kwargs: object) -> None:
        self.data = kwargs.get("data")


def _make_fake_tkinter_module() -> types.ModuleType:
    module = types.ModuleType("tkinter")
    module.TclError = _FakeTclError
    module.Tk = _FakeTkRoot
    module.Canvas = _FakeTkCanvas
    module.PhotoImage = _FakeTkPhotoImage
    return module


class TestTkinterBackendGeometry(unittest.TestCase):
    """Tkinter backend positions windows and survives missing -transparentcolor."""

    def test_open_uses_position_and_falls_back_on_tcl_error(self) -> None:
        fake_tk = _make_fake_tkinter_module()
        with patch.dict(sys.modules, {"tkinter": fake_tk}):
            backend = TkinterOverlayBackend()
            backend.open(320, 240, x=120, y=60)
        root = backend._root
        assert root is not None
        self.assertIn(("320x240+120+60"), root.geometry_calls)
        self.assertTrue(any(c[0] == "-transparentcolor" for c in root.attributes_calls))
        self.assertTrue(any(c[0] == "-alpha" for c in root.attributes_calls))
        self.assertEqual(backend.position, (120, 60))

    def test_move_updates_geometry_suffix(self) -> None:
        fake_tk = _make_fake_tkinter_module()
        with patch.dict(sys.modules, {"tkinter": fake_tk}):
            backend = TkinterOverlayBackend()
            backend.open(320, 240)
            backend.move(400, 250)
        root = backend._root
        assert root is not None
        self.assertTrue(any(g.endswith("+400+250") for g in root.geometry_calls))
        self.assertEqual(backend.position, (400, 250))


class TestPipelineControlStateAndTermination(unittest.TestCase):
    """LivePipelineConfig accepts control state + terminate event, and runner honors them."""

    def test_config_accepts_control_state_and_terminate_event(self) -> None:
        state = LiveControlState()
        event = threading.Event()
        cfg = LivePipelineConfig(control_state=state, terminate_event=event)
        self.assertIs(cfg.control_state, state)
        self.assertIs(cfg.terminate_event, event)
        self.assertIsNone(LivePipelineConfig().control_state)
        self.assertIsNone(LivePipelineConfig().terminate_event)

    def test_run_live_pipeline_uses_injected_state_and_stops_on_terminate(self) -> None:
        from cs2_vision_access.inference.pipeline import runner as runner_mod

        cv2 = MagicMock()
        control_state = LiveControlState()
        control_state.current_preset = "cyan-black"
        terminate_event = threading.Event()
        cfg = LivePipelineConfig(
            max_frames=10_000,
            headless=True,
            capture=CaptureConfig(source=0),
            control_state=control_state,
            terminate_event=terminate_event,
        )

        with (
            patch.dict("sys.modules", {"cv2": cv2}),
            patch("cs2_vision_access.capture.base.cv2", cv2),
            patch.object(runner_mod, "CaptureManager") as mock_capture_cls,
            patch.object(runner_mod, "DisplayManager") as mock_display_cls,
        ):
            terminate_event.set()
            summary = runner_mod.run_live_pipeline(segmenter=MagicMock(), config=cfg)

        self.assertEqual(summary.termination_reason, "user_stop")
        self.assertEqual(summary.frames_processed, 0)
        self.assertIs(mock_display_cls.call_args.kwargs["control_state"], control_state)
        mock_capture_cls.return_value.read.assert_not_called()


class TestWin32Move(unittest.TestCase):
    """Win32 move() drives SetWindowPos with position, size, and flags."""

    def test_move_calls_setwindowpos(self) -> None:
        backend = Win32OverlayBackend()
        fake_bindings = {"SetWindowPos": MagicMock()}
        backend._bindings = fake_bindings
        backend._window = 12345
        backend._width = 640
        backend._height = 480
        backend.move(200, 100)
        fake_bindings["SetWindowPos"].assert_called_once_with(
            12345,
            -1,  # HWND_TOPMOST
            200,
            100,
            640,
            480,
            Win32OverlayBackend._SWP_NOACTIVATE | Win32OverlayBackend._SWP_SHOWWINDOW,
        )
        self.assertEqual(backend.position, (200, 100))


if __name__ == "__main__":
    unittest.main()
