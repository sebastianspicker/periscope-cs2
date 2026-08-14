"""Desktop GUI dashboard/launcher for the live outline pipeline.

Importing this package never touches tkinter (and therefore never requires a
display); the tkinter window is only created when ``run_gui`` is called.
"""

from __future__ import annotations

from cs2_vision_access.gui.app import run_gui
from cs2_vision_access.gui.controller import (
    GuiPipelineController,
    build_live_pipeline_config,
)
from cs2_vision_access.gui.model import GuiSettings

__all__ = [
    "GuiPipelineController",
    "GuiSettings",
    "build_live_pipeline_config",
    "run_gui",
]
