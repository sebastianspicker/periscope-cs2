"""Thin re-export shim for ``overlay_backends`` package.

Existing imports of ``cs2_vision_access.capture._overlay_backends`` continue
to work without change.
"""

from __future__ import annotations

from cs2_vision_access.capture.overlay_backends import (
    CocoaOverlayBackend,
    OverlayBackend,
    TkinterOverlayBackend,
    Win32OverlayBackend,
    detect_platform,
    is_overlay_available,
)

__all__ = [
    "OverlayBackend",
    "Win32OverlayBackend",
    "CocoaOverlayBackend",
    "TkinterOverlayBackend",
    "detect_platform",
    "is_overlay_available",
]
