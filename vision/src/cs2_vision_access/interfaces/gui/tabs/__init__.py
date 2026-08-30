"""Tab-building mixins for the desktop GUI dashboard."""

from __future__ import annotations

from cs2_vision_access.interfaces.gui.tabs.capture import CaptureTabMixin
from cs2_vision_access.interfaces.gui.tabs.model import ModelTabMixin
from cs2_vision_access.interfaces.gui.tabs.overlay import OverlayTabMixin
from cs2_vision_access.interfaces.gui.tabs.run import RunTabMixin
from cs2_vision_access.interfaces.gui.tabs.style import StyleTabMixin

__all__ = [
    "CaptureTabMixin",
    "ModelTabMixin",
    "OverlayTabMixin",
    "RunTabMixin",
    "StyleTabMixin",
]
