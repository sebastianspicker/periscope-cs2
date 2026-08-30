"""Preview-frame encoding helpers for the desktop GUI (no tkinter dependency)."""

from __future__ import annotations

from typing import Any

import numpy as np
from numpy.typing import NDArray

from cs2_vision_access.interfaces.gui.constants import PREVIEW_MAX_WIDTH


def _as_uint8(array: Any) -> NDArray[np.uint8]:
    """Normalize OpenCV's broad array return types to an 8-bit image."""
    return np.asarray(array, dtype=np.uint8)


def to_preview_bytes(frame: Any, max_width: int = PREVIEW_MAX_WIDTH) -> bytes | None:
    """Downscale a rendered frame to PNG bytes for ``tk.PhotoImage``.

    Accepts BGR 3-channel frames and RGBA 4-channel frames (composited over
    black). Never raises: the worker thread must not crash on a bad frame.
    """
    import cv2

    try:
        array = np.asarray(frame)
        if array.ndim != 3 or array.shape[2] not in (3, 4):
            return None
        if array.shape[2] == 4:
            color = array[..., :3].astype(np.float32)
            alpha = array[..., 3:4].astype(np.float32) / 255.0
            composited = _as_uint8(color * alpha)
            composited = _as_uint8(cv2.cvtColor(composited, cv2.COLOR_RGB2BGR))
        else:
            composited = _as_uint8(array)
        height, width = composited.shape[:2]
        if width > max_width:
            scale = max_width / width
            composited = _as_uint8(cv2.resize(composited, (max_width, max(1, int(height * scale)))))
        ok, encoded = cv2.imencode(".png", composited)
        if not ok:
            return None
        return bytes(encoded.tobytes())
    except Exception:
        return None
