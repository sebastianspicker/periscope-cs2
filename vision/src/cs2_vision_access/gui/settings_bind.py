"""Bind GuiSettings to tkinter variables and reverse (no top-level tkinter)."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.gui.model import GuiSettings
from cs2_vision_access.renderer import (
    OUTLINE_PRESET_DESCRIPTIONS,
    OUTLINE_PRESETS,
    get_outline_preset,
)


class SettingsBindMixin:
    """Apply / read settings, parsers, and outline-preset handlers."""

    def _apply_settings(self, settings: GuiSettings) -> None:
        self._source_type.set(settings.source_type)
        self._region_override = settings.region
        self._monitor_var.set(self._monitor_label(settings.monitor_index))
        self._device_var.set(self._device_label(settings.device_index))
        self._file_path.set(settings.file_path)
        self._cap_width.set(str(settings.width))
        self._cap_height.set(str(settings.height))
        self._cap_fps.set(str(settings.fps))
        self._backend.set(settings.backend)

        self._model_path.set(settings.model_path)
        self._manifest_path.set(settings.manifest_path)
        self._seg_backend.set(settings.segmenter_backend)
        self._class_names.set(", ".join(settings.class_names))
        self._confidence.set(settings.confidence)
        self._image_size.set(str(settings.image_size))
        self._device.set(settings.device)

        preset_name = settings.outline_preset
        if preset_name not in OUTLINE_PRESETS:
            preset_name = "maximum-visibility"
        self._preset.set(preset_name)
        self._update_preset_description()
        base = get_outline_preset(preset_name)
        self._inner_width.set(
            settings.inner_width if settings.inner_width is not None else base.inner_width
        )
        self._outer_width.set(
            settings.outer_width if settings.outer_width is not None else base.outer_width
        )
        self._fill_opacity.set(
            settings.fill_opacity if settings.fill_opacity is not None else base.fill_opacity
        )
        self._inner_color.set(settings.inner_color or "")
        self._outer_color.set(settings.outer_color or "")
        self._stroke_pattern.set(settings.stroke_pattern or base.stroke_pattern)
        self._dash_period.set(settings.dash_period or base.dash_period_px)
        self._fill_mode.set(settings.fill_mode or base.fill_mode)
        self._halo_blur.set(settings.halo_blur or base.halo_blur)

        self._adapt_width.set(settings.adapt_width)
        self._fixed_widths.set(settings.fixed_widths)
        self._overlay_enabled.set(settings.overlay)
        self._overlay_x.set(settings.overlay_x)
        self._overlay_y.set(settings.overlay_y)
        self._overlay_monitor.set(settings.overlay_monitor)
        self._window_title.set(settings.window_title)
        self._output_mode.set(settings.output_mode)
        self._alpha_fill.set(settings.alpha_fill)
        self._temporal_enabled.set(settings.temporal_enabled)
        self._temporal_min_frames.set(settings.temporal_min_frames)
        self._temporal_hold.set(settings.temporal_hold)
        self._temporal_max_dropout.set(settings.temporal_max_dropout)
        self._display_scale.set(settings.display_scale)
        self._output_sink.set(settings.output_sink or "")
        self._headless.set(settings.headless)

    def _read_settings(self) -> GuiSettings:
        preset_name = self._preset.get()
        preset_style = get_outline_preset(preset_name)

        inner_raw = self._inner_width.get()
        outer_raw = self._outer_width.get()
        inner_width: int | None = None if inner_raw == preset_style.inner_width else inner_raw
        outer_width: int | None = None if outer_raw == preset_style.outer_width else outer_raw
        fill_opacity = self._fill_opacity.get()
        fill_opacity_override: float | None = (
            None if fill_opacity == preset_style.fill_opacity else fill_opacity
        )
        stroke_pattern = self._stroke_pattern.get()
        stroke_pattern_override: str | None = (
            None if stroke_pattern == preset_style.stroke_pattern else stroke_pattern
        )
        dash_period = max(1, self._dash_period.get())
        dash_override: int | None = (
            None if dash_period == preset_style.dash_period_px else dash_period
        )
        fill_mode = self._fill_mode.get()
        fill_mode_override: str | None = None if fill_mode == preset_style.fill_mode else fill_mode
        halo_blur = max(1, self._halo_blur.get())
        halo_override: int | None = None if halo_blur == preset_style.halo_blur else halo_blur

        class_names_text = self._class_names.get()
        class_names = tuple(
            name.strip() for name in class_names_text.split(",") if name.strip()
        ) or ("person",)

        return GuiSettings(
            source_type=self._source_type.get(),
            monitor_index=self._selected_monitor_index() or 1,
            region=self._region_override,
            device_index=self._selected_device_index() or 0,
            file_path=self._file_path.get().strip(),
            width=self._parse_int(self._cap_width.get(), 1920),
            height=self._parse_int(self._cap_height.get(), 1080),
            fps=self._parse_float(self._cap_fps.get(), 60.0),
            backend=self._backend.get(),
            model_path=self._model_path.get().strip(),
            manifest_path=self._manifest_path.get().strip(),
            segmenter_backend=self._seg_backend.get(),
            class_names=class_names,
            confidence=float(self._confidence.get()),
            image_size=self._parse_int(self._image_size.get(), 640),
            device=self._device.get().strip(),
            outline_preset=preset_name,
            inner_color=self._parse_color(self._inner_color.get()),
            outer_color=self._parse_color(self._outer_color.get()),
            inner_width=inner_width,
            outer_width=outer_width,
            fill_opacity=fill_opacity_override,
            stroke_pattern=stroke_pattern_override,
            dash_period=dash_override,
            fill_mode=fill_mode_override,
            halo_blur=halo_override,
            adapt_width=self._adapt_width.get(),
            fixed_widths=self._fixed_widths.get(),
            output_mode=self._output_mode.get(),
            alpha_fill=self._alpha_fill.get(),
            temporal_enabled=self._temporal_enabled.get(),
            temporal_min_frames=max(1, self._temporal_min_frames.get()),
            temporal_hold=self._temporal_hold.get(),
            temporal_max_dropout=max(0, self._temporal_max_dropout.get()),
            display_scale=float(self._display_scale.get()),
            headless=self._headless.get(),
            overlay=self._overlay_enabled.get(),
            overlay_x=self._overlay_x.get(),
            overlay_y=self._overlay_y.get(),
            overlay_monitor=self._overlay_monitor.get(),
            output_sink=self._output_sink.get().strip() or None,
            window_title=self._window_title.get().strip(),
            max_frames=0,
        )

    @staticmethod
    def _parse_int(value: str, default: int) -> int:
        try:
            return int(value)
        except ValueError:
            return default

    @staticmethod
    def _parse_float(value: str, default: float) -> float:
        try:
            return float(value)
        except ValueError:
            return default

    @staticmethod
    def _parse_color(value: str) -> str | None:
        value = value.strip()
        return value or None

    def _update_preset_description(self) -> None:
        self._preset_desc.set(OUTLINE_PRESET_DESCRIPTIONS.get(self._preset.get(), ""))

    def _on_preset_selected(self, _event: Any) -> None:
        self._update_preset_description()
        try:
            base = get_outline_preset(self._preset.get())
        except ValueError:
            return
        self._inner_width.set(base.inner_width)
        self._outer_width.set(base.outer_width)
        self._fill_opacity.set(base.fill_opacity)
