"""Overlay backend protocol and platform-specific implementations.

Each backend implements the ``OverlayBackend`` protocol with lifecycle methods
(open/close), frame display, and event polling.
"""

from __future__ import annotations

from cs2_vision_access.capture.overlay_backends.cocoa import CocoaOverlayBackend
from cs2_vision_access.capture.overlay_backends.protocol import (
    OverlayBackend,
    detect_platform,
    is_overlay_available,
)
from cs2_vision_access.capture.overlay_backends.tkinter_backend import TkinterOverlayBackend
from cs2_vision_access.capture.overlay_backends.win32 import Win32OverlayBackend

__all__ = [
    "OverlayBackend",
    "Win32OverlayBackend",
    "CocoaOverlayBackend",
    "TkinterOverlayBackend",
    "detect_platform",
    "is_overlay_available",
]
