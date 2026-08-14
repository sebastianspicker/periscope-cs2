"""Live HDMI/SDI capture pipeline with real-time contour output and alpha-only rendering.

Capabilities:
  - Live capture device discovery and selection
  - Real-time inference + outline rendering at capture framerate
  - Alpha-only output mode (transparent background, just outlines)
  - Configurable frame dropping for sustained real-time performance

Usage::

    cs2-vision live --input-device 0 \\
        --model artifacts/yolo26n-seg.onnx \\
        --manifest artifacts/yolo26n-seg.model.json \\
        --class-name person \\
        --outline-preset maximum-visibility \\
        --alpha-only
"""

from __future__ import annotations

from cs2_vision_access.capture.alpha_renderer import (
    AlphaRenderer,
    AlphaRendererConfig,
)
from cs2_vision_access.capture.base import (
    CaptureConfig,
    CaptureDeviceInfo,
    FrameTimingDiagnostics,
    LiveCaptureError,
    list_capture_devices,
    open_capture,
)
from cs2_vision_access.capture.display import DisplayManager
from cs2_vision_access.capture.hotkeys import (
    LiveControlState,
    format_diagnostic_text,
    handle_live_key,
)
from cs2_vision_access.capture.manager import CaptureError, CaptureInfo, CaptureManager
from cs2_vision_access.capture.outputs import FileOutputSink, OutputSink
from cs2_vision_access.capture.overlay import (
    OverlayWindow,
    create_overlay,
    is_overlay_available,
)
from cs2_vision_access.capture.screen import (
    ScreenCaptureError,
    ScreenCapturer,
    ScreenRegion,
    list_monitors,
)

__all__ = [
    "AlphaRenderer",
    "AlphaRendererConfig",
    "CaptureConfig",
    "CaptureDeviceInfo",
    "CaptureError",
    "CaptureInfo",
    "CaptureManager",
    "DisplayManager",
    "FileOutputSink",
    "FrameTimingDiagnostics",
    "LiveCaptureError",
    "LiveControlState",
    "OutputSink",
    "OverlayWindow",
    "ScreenCaptureError",
    "ScreenCapturer",
    "ScreenRegion",
    "create_overlay",
    "format_diagnostic_text",
    "handle_live_key",
    "is_overlay_available",
    "list_capture_devices",
    "list_monitors",
    "open_capture",
]
