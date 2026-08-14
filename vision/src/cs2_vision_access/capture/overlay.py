"""Cross-platform transparent overlay window for live RGBA outline frames.

This module is a thin facade over ``_overlay_backends.py`` which contains the
three platform-specific implementations:

- **Windows**: Win32 layered window via ``ctypes`` — per-pixel alpha,
  click-through, always-on-top.
- **macOS**: Cocoa NSWindow via PyObjC — per-pixel alpha, click-through,
  Retina-aware, all-Spaces.
- **Other / fallback**: tkinter borderless window with best-effort transparency.
"""

from __future__ import annotations

from collections.abc import Callable

import numpy as np

from cs2_vision_access.capture._overlay_backends import (
    CocoaOverlayBackend,
    TkinterOverlayBackend,
    Win32OverlayBackend,
    detect_platform,
    is_overlay_available,
)


class OverlayWindow:
    """Cross-platform transparent overlay window.

    Automatically selects the best backend for the current platform and
    delegates all operations to it.  Usage::

        overlay = OverlayWindow("My Overlay")
        overlay.open(1920, 1080, x=100, y=50)
        overlay.show_frame(rgba_frame)
        overlay.poll_events()
        overlay.close()
    """

    def __init__(
        self,
        title: str = "CS2 Vision Access Overlay",
        *,
        backend: Win32OverlayBackend | CocoaOverlayBackend | TkinterOverlayBackend | None = None,
    ) -> None:
        self._title = title
        self._x = 0
        self._y = 0
        self._backend = backend if backend is not None else _create_backend()

    @property
    def is_open(self) -> bool:
        """Whether the backend window is currently open."""
        return self._backend is not None and self._backend.is_open

    @property
    def position(self) -> tuple[int, int]:
        """Current window position in screen coordinates."""
        backend_position = getattr(self._backend, "position", None)
        if backend_position is not None:
            return (int(backend_position[0]), int(backend_position[1]))
        return (self._x, self._y)

    # -- Lifecycle -----------------------------------------------------------

    def open(self, width: int, height: int, *, x: int = 0, y: int = 0) -> None:
        """Create and show the overlay window."""
        self._x = x
        self._y = y
        if self._backend is not None:
            # open() is idempotent per backend — callers may call it multiple times
            self._backend.open(width, height, self._title, x, y)

    def move(self, x: int, y: int) -> None:
        """Reposition the window without resizing."""
        self._x = x
        self._y = y
        if self._backend is not None:
            self._backend.move(x, y)

    def close(self) -> None:
        """Close and release the overlay window."""
        if self._backend is not None:
            self._backend.close()

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        """Display an RGBA numpy array on the overlay."""
        if self._backend is not None:
            self._backend.show_frame(frame_rgba)

    def poll_events(self) -> bool:
        """Process queued UI events; returns ``False`` after closure."""
        if self._backend is not None:
            return self._backend.poll_events()
        return False

    def set_hotkey_handler(self, handler: Callable[[str], bool] | None) -> None:
        """Install a handler for overlay hotkey actions (backend-specific).

        The handler receives an action name from :data:`hotkeys.HOTKEY_ACTIONS`
        and returns ``True`` if the application should quit.
        """
        if self._backend is not None:
            setter = getattr(self._backend, "set_hotkey_handler", None)
            if setter is not None:
                setter(handler)


def _create_backend() -> Win32OverlayBackend | CocoaOverlayBackend | TkinterOverlayBackend | None:
    """Select and instantiate the best available overlay backend."""
    plat = detect_platform()
    if plat == "win32":
        return Win32OverlayBackend()
    if plat == "darwin":
        try:
            return CocoaOverlayBackend()
        except ImportError:
            pass
    try:
        return TkinterOverlayBackend()
    except ImportError:
        return None


def create_overlay(
    title: str = "CS2 Vision Access Overlay",
    width: int = 1920,
    height: int = 1080,
    *,
    x: int = 0,
    y: int = 0,
) -> OverlayWindow:
    """Create and open an :class:`OverlayWindow`."""
    overlay = OverlayWindow(title)
    overlay.open(width, height, x=x, y=y)
    return overlay


__all__ = [
    "OverlayWindow",
    "create_overlay",
    "is_overlay_available",
]
