"""Tests for the Tkinter overlay backend: alpha color-keying and hotkeys.

No display is required — tkinter and cv2 are stubbed out of ``sys.modules``
following the ``test_overlay_positioning.py`` / ``test_live.py`` pattern.
"""

from __future__ import annotations

import sys
import types
import unittest
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.capture.overlay_backends.tkinter_backend import TkinterOverlayBackend


class EncodePhotoAlphaKeyingTests(unittest.TestCase):
    def test_low_alpha_forced_black_and_compositing(self) -> None:
        frame = np.zeros((8, 8, 4), dtype=np.uint8)
        frame[0, 0] = [255, 128, 64, 0]  # alpha 0 -> black
        frame[1, 1] = [100, 150, 200, 255]  # opaque -> colour kept
        frame[2, 2] = [128, 128, 128, 128]  # half alpha -> composited over black
        frame[3, 3] = [255, 255, 255, 8]  # alpha == threshold -> black
        frame[4, 4] = [255, 255, 255, 9]  # alpha > threshold -> kept

        cv2 = MagicMock()
        cv2.imencode.return_value = (True, b"png")
        fake_tk = SimpleNamespace(PhotoImage=MagicMock(return_value="photo"))
        backend = TkinterOverlayBackend()

        with patch.dict(sys.modules, {"cv2": cv2}):
            result = backend._encode_photo(fake_tk, frame)

        self.assertEqual(result, "photo")
        cv2.imencode.assert_called_once()
        encoded = cv2.imencode.call_args[0][1]
        self.assertEqual(encoded.ndim, 3)
        self.assertEqual(encoded.shape[2], 3)
        self.assertEqual(encoded.dtype, np.uint8)
        np.testing.assert_array_equal(encoded[0, 0], [0, 0, 0])
        np.testing.assert_array_equal(encoded[1, 1], [100, 150, 200])
        np.testing.assert_array_equal(encoded[2, 2], [64, 64, 64])
        np.testing.assert_array_equal(encoded[3, 3], [0, 0, 0])
        np.testing.assert_array_equal(encoded[4, 4], [9, 9, 9])

    def test_three_channel_input_passthrough(self) -> None:
        frame = np.full((4, 4, 3), 200, dtype=np.uint8)
        frame[1, 1] = [10, 20, 30]
        cv2 = MagicMock()
        cv2.imencode.return_value = (True, b"png")
        fake_tk = SimpleNamespace(PhotoImage=MagicMock(return_value="photo"))
        backend = TkinterOverlayBackend()

        with patch.dict(sys.modules, {"cv2": cv2}):
            backend._encode_photo(fake_tk, frame)

        encoded = cv2.imencode.call_args[0][1]
        self.assertEqual(encoded.shape, (4, 4, 3))
        np.testing.assert_array_equal(encoded, frame)


class HotkeyDispatchTests(unittest.TestCase):
    def setUp(self) -> None:
        self.backend = TkinterOverlayBackend()
        self.handler = MagicMock(return_value=False)
        self.backend.set_hotkey_handler(self.handler)

    def _press(self, char: str = "", keysym: str = "") -> None:
        self.backend._on_key_press(SimpleNamespace(char=char, keysym=keysym))

    def test_mapped_keys_dispatch_actions(self) -> None:
        cases = [
            (("q", "q"), "quit"),
            (("Q", "Q"), "quit"),
            ((" ", "space"), "pause"),
            (("1", "1"), "preset-1"),
            (("=", "equal"), "width-up"),
            (("t", "t"), "temporal"),
            (("o", "o"), "mode-cycle"),
            (("f", "f"), "fill"),
            (("h", "h"), "diagnostics"),
            (("", "Escape"), "quit"),
            (("", "space"), "pause"),
            (("", "plus"), "width-up"),
        ]
        for (char, keysym), action in cases:
            with self.subTest(char=char, keysym=keysym):
                self.handler.reset_mock()
                self._press(char, keysym)
                self.handler.assert_called_once_with(action)

    def test_unknown_keys_do_not_dispatch(self) -> None:
        for char, keysym in [("x", "x"), ("", "F1"), ("9", "9")]:
            with self.subTest(char=char, keysym=keysym):
                self.handler.reset_mock()
                self._press(char, keysym)
                self.handler.assert_not_called()

    def test_handler_returning_true_closes_backend(self) -> None:
        self.handler.return_value = True
        root = MagicMock()
        self.backend._root = root
        self._press("q", "q")
        self.handler.assert_called_once_with("quit")
        root.destroy.assert_called_once()
        self.assertIsNone(self.backend._root)
        self.assertFalse(self.backend.is_open)


class HotkeyNoHandlerTests(unittest.TestCase):
    def test_no_handler_does_nothing(self) -> None:
        backend = TkinterOverlayBackend()
        backend._on_key_press(SimpleNamespace(char="q", keysym="q"))
        self.assertIsNone(backend._hotkey_handler)
        self.assertFalse(backend.is_open)


class _FakeTclError(Exception):
    pass


class _RecordingRoot:
    def __init__(self) -> None:
        self.attributes_calls: list[tuple[object, ...]] = []
        self.bind_calls: list[tuple[str, object]] = []
        self.focus_force_calls = 0
        self.geometry_calls: list[str] = []

    def title(self, title: str) -> None:
        del title

    def overrideredirect(self, flag: bool) -> None:
        del flag

    def attributes(self, *args: object) -> object:
        self.attributes_calls.append(args)
        return True

    def geometry(self, geometry: str) -> None:
        self.geometry_calls.append(geometry)

    def configure(self, **kwargs: object) -> None:
        del kwargs

    def bind(self, sequence: str, handler: object) -> None:
        self.bind_calls.append((sequence, handler))

    def focus_force(self) -> None:
        self.focus_force_calls += 1

    def destroy(self) -> None:
        pass


class _BareRoot:
    """Root without bind()/focus_force() — the binding must be best-effort."""

    def __init__(self) -> None:
        self.attributes_calls: list[tuple[object, ...]] = []
        self.geometry_calls: list[str] = []

    def title(self, title: str) -> None:
        del title

    def overrideredirect(self, flag: bool) -> None:
        del flag

    def attributes(self, *args: object) -> object:
        self.attributes_calls.append(args)
        return True

    def geometry(self, geometry: str) -> None:
        self.geometry_calls.append(geometry)

    def configure(self, **kwargs: object) -> None:
        del kwargs


class _FakeCanvas:
    def __init__(self, *args: object, **kwargs: object) -> None:
        del args, kwargs

    def pack(self) -> None:
        pass


def _make_fake_tk(root: object) -> types.ModuleType:
    module = types.ModuleType("tkinter")
    module.TclError = _FakeTclError
    module.Tk = MagicMock(return_value=root)
    module.Canvas = _FakeCanvas
    module.PhotoImage = MagicMock()
    return module


class TkinterBackendOpenTests(unittest.TestCase):
    def test_open_binds_keypress_and_focuses(self) -> None:
        root = _RecordingRoot()
        with patch.dict(sys.modules, {"tkinter": _make_fake_tk(root)}):
            backend = TkinterOverlayBackend()
            backend.open(320, 240, x=10, y=20)
        self.assertTrue(backend.is_open)
        self.assertIs(backend._root, root)
        self.assertIn(("<KeyPress>", backend._on_key_press), root.bind_calls)
        self.assertEqual(root.focus_force_calls, 1)
        self.assertTrue(any(c[0] == "-transparentcolor" for c in root.attributes_calls))

    def test_open_survives_missing_bind_and_focus_force(self) -> None:
        root = _BareRoot()
        with patch.dict(sys.modules, {"tkinter": _make_fake_tk(root)}):
            backend = TkinterOverlayBackend()
            backend.open(320, 240)
        self.assertTrue(backend.is_open)
        self.assertIs(backend._root, root)
        self.assertTrue(any(c[0] == "-transparentcolor" for c in root.attributes_calls))


if __name__ == "__main__":
    unittest.main()
