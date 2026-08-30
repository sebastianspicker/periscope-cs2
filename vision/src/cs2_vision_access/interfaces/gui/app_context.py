"""Static contract shared by the dynamically composed tkinter GUI mixins.

``GuiApp`` combines tab, lifecycle, source, and widget mixins at runtime.
The contract records the members that those mixins deliberately share without
importing tkinter when the package is imported in a headless environment.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any, Protocol

if TYPE_CHECKING:
    from cs2_vision_access.interfaces.gui.controller import GuiPipelineController


class GuiAppContext(Protocol):
    """Members supplied by :class:`GuiApp` to its cooperating mixins."""

    _adapt_width: Any
    _alpha_fill: Any
    _backend: Any
    _cap_fps: Any
    _cap_height: Any
    _cap_width: Any
    _class_names: Any
    _confidence: Any
    _controller: GuiPipelineController | None
    _current_settings: Any
    _dash_period: Any
    _device: Any
    _device_combo: Any
    _device_indices: list[int]
    _device_labels: list[str]
    _device_var: Any
    _diag_var: Any
    _display_scale: Any
    _file_path: Any
    _filedialog: Any
    _fill_mode: Any
    _fill_opacity: Any
    _fixed_widths: Any
    _halo_blur: Any
    _headless: Any
    _hotkey_buttons: Any
    _image_size: Any
    _inner_color: Any
    _inner_width: Any
    _last_photo: Any
    _log_text: Any
    _manifest_path: Any
    _messagebox: Any
    _model_path: Any
    _monitor_combo: Any
    _monitor_indices: list[int]
    _monitor_labels: list[str]
    _monitor_var: Any
    _outer_color: Any
    _outer_width: Any
    _output_mode: Any
    _output_sink: Any
    _overlay_enabled: Any
    _overlay_monitor: Any
    _overlay_x: Any
    _overlay_y: Any
    _preset: Any
    _preset_desc: Any
    _preview_canvas: Any
    _preview_image_id: Any
    _preview_queue: Any
    _region_override: tuple[int, ...] | None
    _root: Any
    _seg_backend: Any
    _source_type: Any
    _start_button: Any
    _started_at: Any
    _status_var: Any
    _stop_button: Any
    _stroke_pattern: Any
    _summary_text: Any
    _temporal_enabled: Any
    _temporal_hold: Any
    _temporal_max_dropout: Any
    _temporal_min_frames: Any
    _tk: Any
    _ttk: Any
    _window_title: Any

    def _apply_settings(self, settings: Any) -> None: ...

    def _browse_file(self) -> None: ...

    def _browse_manifest(self) -> None: ...

    def _browse_model(self) -> None: ...

    def _browse_sink(self) -> None: ...

    def _device_label(self, index: int) -> str: ...

    def _grid_pair(self, parent: Any, row: int, text: str, variable: Any) -> None: ...

    def _log(self, message: str) -> None: ...

    def _monitor_label(self, index: int) -> str: ...

    def _on_monitor_selected(self, event: Any) -> None: ...

    def _on_preset_selected(self, event: Any) -> None: ...

    def _read_settings(self) -> Any: ...

    def _refresh_devices(self) -> None: ...

    def _refresh_monitors(self) -> None: ...

    def _scale_row(
        self,
        parent: Any,
        row: int,
        text: str,
        variable: Any,
        from_: float,
        to: float,
        resolution: float,
        fmt: str,
    ) -> None: ...

    def _selected_device_index(self) -> int | None: ...

    def _selected_monitor_index(self) -> int | None: ...

    def _set_hotkey_buttons(self, state: str) -> None: ...

    def apply_hotkey_button(self, action: str) -> None: ...

    def load_config(self) -> None: ...

    def reset_settings(self) -> None: ...

    def save_config(self) -> None: ...

    def start_live(self) -> None: ...

    def stop_live(self) -> None: ...
