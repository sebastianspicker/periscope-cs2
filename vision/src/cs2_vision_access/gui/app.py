"""tkinter dashboard/launcher for the live outline pipeline.

Import-safe without a display: ``tkinter`` is only imported inside ``GuiApp``
methods and ``run_gui``. The settings model and pipeline controller live in
sibling modules that never touch tkinter.
"""

from __future__ import annotations

import queue
import sys
from typing import TYPE_CHECKING, Any

from cs2_vision_access.gui.constants import PREVIEW_POLL_MS, PREVIEW_QUEUE_MAX
from cs2_vision_access.gui.controller import GuiPipelineController
from cs2_vision_access.gui.lifecycle import LifecycleMixin
from cs2_vision_access.gui.model import GuiSettings
from cs2_vision_access.gui.settings_bind import SettingsBindMixin
from cs2_vision_access.gui.sources import SourceEnumerationMixin
from cs2_vision_access.gui.tabs import (
    CaptureTabMixin,
    ModelTabMixin,
    OverlayTabMixin,
    RunTabMixin,
    StyleTabMixin,
)
from cs2_vision_access.gui.widgets import WidgetHelpersMixin
from cs2_vision_access.segmenters import DEFAULT_SEGMENTER_BACKEND

if TYPE_CHECKING:
    import tkinter as tk


def run_gui(settings: GuiSettings | None = None) -> int:
    """Launch the desktop dashboard; 0 on clean exit, 2 when no display exists."""
    try:
        import tkinter as tk
    except ImportError as error:
        print(f"gui: tkinter is not available: {error}", file=sys.stderr)
        return 2
    try:
        root = tk.Tk()
    except tk.TclError as error:
        print(f"gui: could not open a display (TclError): {error}", file=sys.stderr)
        return 2
    app = GuiApp(root, settings=settings)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
    return 0


class GuiApp(
    CaptureTabMixin,
    ModelTabMixin,
    StyleTabMixin,
    OverlayTabMixin,
    RunTabMixin,
    WidgetHelpersMixin,
    SourceEnumerationMixin,
    SettingsBindMixin,
    LifecycleMixin,
):
    """Main tkinter window: five-tab dashboard driving the live pipeline."""

    def __init__(self, root: tk.Tk, settings: GuiSettings | None = None) -> None:
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk

        self._tk = tk
        self._ttk = ttk
        self._filedialog = filedialog
        self._messagebox = messagebox
        self._root = root
        self._controller: GuiPipelineController | None = None
        self._preview_queue: queue.Queue[tuple[bytes, int, int]] = queue.Queue(
            maxsize=PREVIEW_QUEUE_MAX
        )
        self._last_photo: Any = None
        self._started_at = 0.0
        self._region_override: tuple[int, ...] | None = None
        self._monitor_indices: list[int] = []
        self._monitor_labels: list[str] = []
        self._device_indices: list[int] = []
        self._device_labels: list[str] = []
        self._hotkey_buttons: list[Any] = []
        self._current_settings = settings if settings is not None else GuiSettings()

        self._create_variables()
        root.title("CS2 Vision Access - Live Outline Dashboard")
        root.geometry("980x760")

        notebook = ttk.Notebook(root)
        notebook.pack(fill="both", expand=True, padx=6, pady=6)

        self._build_capture_tab(notebook)
        self._build_model_tab(notebook)
        self._build_style_tab(notebook)
        self._build_overlay_tab(notebook)
        self._build_run_tab(notebook)

        self._apply_settings(self._current_settings)
        root.after(PREVIEW_POLL_MS, self._poll_preview)

    def _create_variables(self) -> None:
        tk = self._tk
        root = self._root
        self._source_type = tk.StringVar(root, "screen")
        self._monitor_var = tk.StringVar(root, "")
        self._device_var = tk.StringVar(root, "")
        self._file_path = tk.StringVar(root, "")
        self._cap_width = tk.StringVar(root, "1920")
        self._cap_height = tk.StringVar(root, "1080")
        self._cap_fps = tk.StringVar(root, "60.0")
        self._backend = tk.StringVar(root, "auto")

        self._model_path = tk.StringVar(root, "artifacts/yolo26n-seg.onnx")
        self._manifest_path = tk.StringVar(root, "artifacts/yolo26n-seg.model.json")
        self._seg_backend = tk.StringVar(root, DEFAULT_SEGMENTER_BACKEND)
        self._class_names = tk.StringVar(root, "person")
        self._confidence = tk.DoubleVar(root, 0.45)
        self._image_size = tk.StringVar(root, "640")
        self._device = tk.StringVar(root, "cpu")

        self._preset = tk.StringVar(root, "maximum-visibility")
        self._preset_desc = tk.StringVar(root, "")
        self._inner_color = tk.StringVar(root, "")
        self._outer_color = tk.StringVar(root, "")
        self._inner_width = tk.IntVar(root, 3)
        self._outer_width = tk.IntVar(root, 7)
        self._fill_opacity = tk.DoubleVar(root, 0.08)
        self._stroke_pattern = tk.StringVar(root, "solid")
        self._dash_period = tk.IntVar(root, 12)
        self._fill_mode = tk.StringVar(root, "tint")
        self._halo_blur = tk.IntVar(root, 9)
        self._adapt_width = tk.BooleanVar(root, False)
        self._fixed_widths = tk.BooleanVar(root, False)

        self._overlay_enabled = tk.BooleanVar(root, False)
        self._overlay_x = tk.IntVar(root, 0)
        self._overlay_y = tk.IntVar(root, 0)
        self._overlay_monitor = tk.IntVar(root, 0)
        self._window_title = tk.StringVar(root, "CS2 Vision Access - Ingame Overlay")
        self._output_mode = tk.StringVar(root, "overlay")
        self._alpha_fill = tk.BooleanVar(root, False)
        self._temporal_enabled = tk.BooleanVar(root, False)
        self._temporal_min_frames = tk.IntVar(root, 2)
        self._temporal_hold = tk.BooleanVar(root, False)
        self._temporal_max_dropout = tk.IntVar(root, 0)
        self._display_scale = tk.DoubleVar(root, 0.5)
        self._output_sink = tk.StringVar(root, "")
        self._headless = tk.BooleanVar(root, False)

        self._diag_var = tk.StringVar(root, "Idle")
        self._status_var = tk.StringVar(root, "Stopped")
