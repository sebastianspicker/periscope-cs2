"""Supervision bridge — re-exports from ``renderer.supervision``.

This file exists for backward compatibility. Prefer importing from
``cs2_vision_access.renderer.supervision`` directly.
"""

from __future__ import annotations

from cs2_vision_access.renderer import supervision as _supervision
from cs2_vision_access.renderer.supervision import (  # noqa: F401
    SupervisionAnnotator,
    SupervisionAnnotatorConfig,
    SupervisionBridgeError,
    detections_to_instance_masks,
    instance_masks_to_detections,
)

# Re-export optional deps under this module name so existing tests can patch
# ``cs2_vision_access.supervision_bridge.sv`` / ``.cv2``.
cv2 = _supervision.cv2
sv = _supervision.sv

__all__ = [
    "SupervisionAnnotator",
    "SupervisionAnnotatorConfig",
    "SupervisionBridgeError",
    "cv2",
    "detections_to_instance_masks",
    "instance_masks_to_detections",
    "sv",
]
