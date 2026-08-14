"""Pure data model for the desktop GUI dashboard (no tkinter dependency).

Holds every live-pipeline knob in a plain dataclass and converts to/from the
persistent ``AppConfig`` hierarchy so the GUI can load and save JSON configs.
"""

from __future__ import annotations

from dataclasses import dataclass

from cs2_vision_access.config import (
    AppConfig,
    DisplayConfig,
    InputConfig,
    ModelConfig,
    OutlineConfig,
)
from cs2_vision_access.segmenters import (
    DEFAULT_SEGMENTER_BACKEND,
    Segmenter,
    create_segmenter,
)


@dataclass
class GuiSettings:
    """All settings surfaced by the desktop dashboard.

    Mirrors ``AppConfig`` plus the style/live knobs that the live CLI accepts
    as flags (dash period, fill mode, halo blur, adaptive widths, max frames,
    output sink). Fields not present in ``AppConfig`` are simply not persisted.
    """

    source_type: str = "screen"
    monitor_index: int = 1
    region: tuple[int, ...] | None = None
    device_index: int = 0
    file_path: str = ""
    width: int = 1920
    height: int = 1080
    fps: float = 60.0
    backend: str = "auto"
    model_path: str = "artifacts/yolo26n-seg.onnx"
    manifest_path: str = "artifacts/yolo26n-seg.model.json"
    segmenter_backend: str = DEFAULT_SEGMENTER_BACKEND
    class_names: tuple[str, ...] = ("person",)
    confidence: float = 0.45
    image_size: int = 640
    device: str = "cpu"
    outline_preset: str = "maximum-visibility"
    inner_color: str | None = None
    outer_color: str | None = None
    inner_width: int | None = None
    outer_width: int | None = None
    fill_opacity: float | None = None
    stroke_pattern: str | None = None
    dash_period: int | None = None
    fill_mode: str | None = None
    halo_blur: int | None = None
    adapt_width: bool = False
    fixed_widths: bool = False
    output_mode: str = "overlay"
    alpha_fill: bool = False
    temporal_enabled: bool = False
    temporal_min_frames: int = 2
    temporal_hold: bool = False
    temporal_max_dropout: int = 0
    display_scale: float = 0.5
    headless: bool = False
    overlay: bool = False
    overlay_x: int = 0
    overlay_y: int = 0
    overlay_monitor: int = 0
    output_sink: str | None = None
    window_title: str = "CS2 Vision Access - Ingame Overlay"
    max_frames: int = 0

    @classmethod
    def from_config(cls, config: AppConfig) -> GuiSettings:
        """Build settings from a persisted ``AppConfig``.

        ``DisplayConfig`` is extended in parallel with overlay positioning
        fields; they are read defensively so older configs still load.
        """
        inp = config.input
        mdl = config.model
        outline = config.outline
        display = config.display
        return cls(
            source_type=inp.source_type,
            monitor_index=inp.monitor_index,
            region=inp.region,
            device_index=inp.device_index,
            file_path=inp.file_path,
            width=inp.width,
            height=inp.height,
            fps=inp.fps,
            backend=inp.backend,
            model_path=mdl.path,
            manifest_path=mdl.manifest,
            segmenter_backend=mdl.backend,
            class_names=mdl.class_names,
            confidence=mdl.confidence,
            image_size=mdl.image_size,
            device=mdl.device,
            outline_preset=outline.preset,
            inner_color=outline.inner_color,
            outer_color=outline.outer_color,
            inner_width=outline.inner_width,
            outer_width=outline.outer_width,
            fill_opacity=outline.fill_opacity,
            stroke_pattern=outline.stroke_pattern,
            output_mode=outline.output_mode,
            alpha_fill=outline.alpha_fill,
            temporal_enabled=outline.temporal_enabled,
            temporal_min_frames=outline.temporal_min_frames,
            temporal_hold=outline.temporal_hold,
            temporal_max_dropout=outline.temporal_max_dropout,
            display_scale=display.scale,
            headless=display.headless,
            overlay=display.overlay,
            overlay_x=int(getattr(display, "overlay_x", 0)),
            overlay_y=int(getattr(display, "overlay_y", 0)),
            overlay_monitor=int(getattr(display, "overlay_monitor", 0)),
            window_title=display.window_title,
        )

    def to_config(self) -> AppConfig:
        """Convert settings to a persisted ``AppConfig`` (all frozen dataclasses)."""
        return AppConfig(
            input=InputConfig(
                source_type=self.source_type,
                device_index=self.device_index,
                monitor_index=self.monitor_index,
                region=self.region,
                file_path=self.file_path,
                width=self.width,
                height=self.height,
                fps=self.fps,
                backend=self.backend,
            ),
            model=ModelConfig(
                path=self.model_path,
                manifest=self.manifest_path,
                backend=self.segmenter_backend,
                class_names=self.class_names,
                confidence=self.confidence,
                image_size=self.image_size,
                device=self.device,
            ),
            outline=OutlineConfig(
                preset=self.outline_preset,
                inner_color=self.inner_color,
                outer_color=self.outer_color,
                inner_width=self.inner_width,
                outer_width=self.outer_width,
                fill_opacity=self.fill_opacity,
                stroke_pattern=self.stroke_pattern,
                output_mode=self.output_mode,
                alpha_fill=self.alpha_fill,
                temporal_enabled=self.temporal_enabled,
                temporal_min_frames=self.temporal_min_frames,
                temporal_hold=self.temporal_hold,
                temporal_max_dropout=self.temporal_max_dropout,
            ),
            display=DisplayConfig(
                scale=self.display_scale,
                headless=self.headless,
                window_title=self.window_title,
                overlay=self.overlay,
                overlay_x=self.overlay_x,
                overlay_y=self.overlay_y,
                overlay_monitor=self.overlay_monitor,
            ),
        )


def build_segmenter(settings: GuiSettings) -> Segmenter:
    """Construct the configured segmenter (thin wrapper over the factory)."""
    return create_segmenter(
        settings.model_path,
        settings.manifest_path,
        backend=settings.segmenter_backend,
        class_names=settings.class_names,
        confidence=settings.confidence,
        image_size=settings.image_size,
        device=settings.device,
    )
