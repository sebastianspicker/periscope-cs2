"""Tkinter fallback overlay backend."""

from __future__ import annotations

import logging
from collections.abc import Callable
from typing import Any

import numpy as np

LOGGER = logging.getLogger(__name__)

# Pixels at or below this alpha are forced to pure black so the window's
# ``-transparentcolor black`` color key hides them entirely.
_ALPHA_TRANSPARENT_THRESHOLD = 8.0 / 255.0

# Tk key press -> hotkey action mapping, keyed by ``event.char`` with
# ``event.keysym`` fallbacks for keys that have no printable character.
_HOTKEY_ACTION_KEYS: dict[str, str] = {
    "q": "quit",
    "Q": "quit",
    " ": "pause",
    "1": "preset-1",
    "2": "preset-2",
    "3": "preset-3",
    "=": "width-up",
    "+": "width-up",
    "-": "width-down",
    "_": "width-down",
    "t": "temporal",
    "T": "temporal",
    "o": "mode-cycle",
    "O": "mode-cycle",
    "f": "fill",
    "F": "fill",
    "h": "diagnostics",
    "H": "diagnostics",
    "Escape": "quit",
    "space": "pause",
    "equal": "width-up",
    "plus": "width-up",
    "minus": "width-down",
    "underscore": "width-down",
}


class TkinterOverlayBackend:
    """Tkinter borderless window — best-effort transparency."""

    def __init__(self) -> None:
        self._root: Any = None
        self._canvas: Any = None
        self._image_id: int | None = None
        self._photo: Any = None
        self._width: int = 0
        self._height: int = 0
        self._x: int = 0
        self._y: int = 0
        self._running = False
        self._hotkey_handler: Callable[[str], bool] | None = None

    @property
    def is_open(self) -> bool:
        return self._root is not None

    @property
    def position(self) -> tuple[int, int]:
        """Current window position in screen pixels."""
        return (self._x, self._y)

    def open(
        self,
        width: int,
        height: int,
        title: str = "CS2 Vision Access Overlay",
        x: int = 0,
        y: int = 0,
    ) -> None:
        import tkinter as tk

        self._width = width
        self._height = height
        self._x = int(x)
        self._y = int(y)

        root = tk.Tk()
        root.title(title)
        root.overrideredirect(True)
        root.attributes("-topmost", True)
        try:
            root.attributes("-transparentcolor", "black")
        except tk.TclError:
            # -transparentcolor is Windows-only; fall back to a translucent window.
            root.attributes("-alpha", 0.95)
        root.geometry(f"{width}x{height}+{self._x}+{self._y}")
        root.configure(bg="black")

        canvas = tk.Canvas(root, width=width, height=height, bg="black", highlightthickness=0)
        canvas.pack()
        self._root = root
        self._canvas = canvas
        self._image_id = None
        self._running = True

        try:
            root.bind("<KeyPress>", self._on_key_press)
            root.focus_force()
        except tk.TclError as error:
            # Binding is optional for an overlay whose window manager cannot
            # grant focus; drawing remains usable through the normal pipeline.
            LOGGER.debug("Tkinter hotkeys unavailable; continuing without focus: %s", error)

    def move(self, x: int, y: int) -> None:
        """Reposition the window without resizing."""
        self._x = int(x)
        self._y = int(y)
        if self._root is not None:
            self._root.geometry(f"{self._width}x{self._height}+{self._x}+{self._y}")

    def set_hotkey_handler(self, handler: Callable[[str], bool] | None) -> None:
        """Install a callback invoked with hotkey action names from key presses."""
        self._hotkey_handler = handler

    def close(self) -> None:
        if self._root:
            try:
                import tkinter as tk
            except ModuleNotFoundError:
                # Cleanup can run after a headless fallback has made Tkinter
                # unavailable. There is no live Tcl interpreter to notify.
                self._root = None
            else:
                try:
                    self._root.destroy()
                except tk.TclError as error:
                    # A user/window manager can destroy the window before the
                    # pipeline's cleanup callback runs.
                    LOGGER.debug("Tkinter overlay was already closed: %s", error)
                self._root = None

        self._running = False

    def _on_key_press(self, event: Any) -> None:
        """Map a tkinter ``<KeyPress>`` event to a hotkey action and dispatch it."""
        handler = self._hotkey_handler
        if handler is None:
            return
        char = getattr(event, "char", "")
        keysym = getattr(event, "keysym", "")
        action = _HOTKEY_ACTION_KEYS.get(char) or _HOTKEY_ACTION_KEYS.get(keysym)
        if action is None:
            return
        should_quit = handler(action)
        if should_quit:
            self.close()

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        if not self._running or self._root is None:
            return
        try:
            import tkinter as tk

            self._photo = self._encode_photo(tk, frame_rgba)
            if self._image_id is None:
                self._image_id = self._canvas.create_image(
                    0,
                    0,
                    anchor="nw",
                    image=self._photo,
                )
            else:
                self._canvas.itemconfig(self._image_id, image=self._photo)
            self._root.update_idletasks()
        except tk.TclError as error:
            # Closing the overlay between frames is a normal UI race. Stop
            # rendering instead of hiding encoding/programming failures.
            self._running = False
            LOGGER.debug("Tkinter overlay closed while drawing a frame: %s", error)

    def _encode_photo(self, tk: Any, frame_rgba: np.ndarray) -> Any:
        """Encode an RGBA frame as a ``tk.PhotoImage`` (cv2 preferred, PIL fallback).

        Pixels at or below :data:`_ALPHA_TRANSPARENT_THRESHOLD` alpha are forced
        to pure black so the ``-transparentcolor black`` color key hides them;
        everything else is composited over black.
        """
        if frame_rgba.ndim == 3 and frame_rgba.shape[2] == 4:
            alpha = frame_rgba[:, :, 3:4].astype(np.float32) / 255.0
            bgr = (frame_rgba[:, :, :3].astype(np.float32) * alpha).astype(np.uint8)
            bgr[alpha[:, :, 0] <= _ALPHA_TRANSPARENT_THRESHOLD] = 0
        else:
            bgr = frame_rgba

        cv2: Any
        try:
            import cv2
        except ImportError:
            cv2 = None

        if cv2 is not None:
            ok, png_bytes = cv2.imencode(".png", bgr)
            if ok:
                # OpenCV returns a 1-D ndarray; some stubs/tests return raw bytes.
                if isinstance(png_bytes, (bytes, bytearray, memoryview)):
                    data = bytes(png_bytes)
                else:
                    data = png_bytes.tobytes()
                return tk.PhotoImage(data=data)

        from PIL import Image, ImageTk

        if bgr.ndim == 3 and bgr.shape[2] == 3:
            pil_image = Image.fromarray(bgr[:, :, ::-1], "RGB")
        else:
            pil_image = Image.fromarray(bgr, "RGB")
        return ImageTk.PhotoImage(pil_image)

    def poll_events(self) -> bool:
        if not self._running or self._root is None:
            return False
        import tkinter as tk

        try:
            self._root.update()
        except tk.TclError as error:
            self._running = False
            LOGGER.debug("Tkinter overlay event loop closed: %s", error)
            return False
        return True
