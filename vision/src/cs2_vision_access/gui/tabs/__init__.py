"""Tab-building mixins for the desktop GUI dashboard."""

from __future__ import annotations

from cs2_vision_access.gui.tabs.capture import CaptureTabMixin
from cs2_vision_access.gui.tabs.model import ModelTabMixin
from cs2_vision_access.gui.tabs.overlay import OverlayTabMixin
from cs2_vision_access.gui.tabs.run import RunTabMixin
from cs2_vision_access.gui.tabs.style import StyleTabMixin

__all__ = [
    "CaptureTabMixin",
    "ModelTabMixin",
    "OverlayTabMixin",
    "RunTabMixin",
    "StyleTabMixin",
]
