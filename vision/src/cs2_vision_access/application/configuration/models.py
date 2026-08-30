"""Configuration data models for the cs2-vision live pipeline.

All dataclasses are frozen (immutable) for safe sharing across modules.
"""

from __future__ import annotations

from dataclasses import dataclass, field

CONFIG_FILENAME = "cs2-vision-config.json"
CONFIG_SCHEMA_VERSION = 1


class ConfigError(ValueError):
    """Configuration validation or IO failed."""


@dataclass(frozen=True)
class InputConfig:
    """Capture source configuration.

    ``source_type``: ``"capture-device"``, ``"screen"``, or ``"file"``.
    For ``capture-device``: ``device_index`` (int) or ``device_name`` (str).
    For ``screen``: ``monitor_index`` (1-based), or ``region`` for a sub-region.
    For ``file``: ``file_path`` (path to video file).
    """

    source_type: str = "screen"  # "capture-device" | "screen" | "file"
    device_index: int = 0
    device_name: str = ""
    monitor_index: int = 1
    region: tuple[int, ...] | None = None  # left, top, width, height
    file_path: str = ""
    width: int = 1920
    height: int = 1080
    fps: float = 60.0
    backend: str = "auto"


@dataclass(frozen=True)
class ModelConfig:
    """Trained-model settings for inference."""

    path: str = "artifacts/yolo26n-seg.onnx"
    manifest: str = "artifacts/yolo26n-seg.model.json"
    backend: str = "ultralytics-onnx"
    class_names: tuple[str, ...] = ("person",)
    confidence: float = 0.45
    image_size: int = 640
    device: str = "cpu"


@dataclass(frozen=True)
class OutlineConfig:
    """Outline rendering style and temporal configuration."""

    preset: str = "maximum-visibility"
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
    outline_kernel: str | None = None
    output_mode: str = "overlay"  # "overlay" | "alpha" | "green"
    alpha_fill: bool = False
    temporal_enabled: bool = False
    temporal_min_frames: int = 2
    temporal_hold: bool = False
    temporal_max_dropout: int = 0


@dataclass(frozen=True)
class DisplayConfig:
    """Display-window and overlay preferences.

    ``overlay_x`` / ``overlay_y`` position the overlay window's top-left corner
    in screen pixels (0 = auto-align to the captured monitor or region origin).
    ``overlay_monitor`` selects the 1-based monitor the overlay aligns to
    (0 = auto/primary, derived from the capture source).
    """

    scale: float = 0.5
    headless: bool = False
    window_title: str = "CS2 Vision Access - Ingame Overlay"
    overlay: bool = False  # Use transparent always-on-top overlay window
    overlay_x: int = 0  # Overlay window x offset in screen pixels (0 = auto/primary)
    overlay_y: int = 0  # Overlay window y offset in screen pixels (0 = auto/primary)
    overlay_monitor: int = 0  # 1-based monitor to align the overlay to; 0 = auto/primary
    output_sink: str | None = None
    max_frames: int = 0


@dataclass(frozen=True)
class AppConfig:
    """Complete application configuration serialised to/from a JSON file."""

    schema_version: int = CONFIG_SCHEMA_VERSION
    input: InputConfig = field(default_factory=InputConfig)
    model: ModelConfig = field(default_factory=ModelConfig)
    outline: OutlineConfig = field(default_factory=OutlineConfig)
    display: DisplayConfig = field(default_factory=DisplayConfig)
