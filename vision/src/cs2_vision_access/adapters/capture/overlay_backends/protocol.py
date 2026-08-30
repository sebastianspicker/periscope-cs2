"""Overlay backend protocol and platform detection."""

from __future__ import annotations

import sys
from typing import Protocol

import numpy as np


class OverlayBackend(Protocol):
    """Interface for platform-specific overlay implementations."""

    def open(
        self,
        width: int,
        height: int,
        title: str = "CS2 Vision Access Overlay",
        x: int = 0,
        y: int = 0,
    ) -> None: ...
    def move(self, x: int, y: int) -> None: ...
    def close(self) -> None: ...
    def show_frame(self, frame_rgba: np.ndarray) -> None: ...
    def poll_events(self) -> bool: ...
    @property
    def is_open(self) -> bool: ...


def detect_platform() -> str:
    """Return ``"win32"``, ``"darwin"``, or ``"other"``."""
    if sys.platform == "win32":
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "other"


def is_overlay_available() -> bool:
    """Return whether an overlay backend is available for this platform."""
    plat = detect_platform()
    if plat == "win32":
        return True
    if plat == "darwin":
        try:
            import Quartz  # noqa: F401

            return True
        except ImportError:
            pass
    try:
        import tkinter  # noqa: F401

        return True
    except ImportError:
        return False
