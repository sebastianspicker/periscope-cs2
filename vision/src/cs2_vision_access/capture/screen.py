"""Screen/window capture for ingame overlay without an HDMI capture card.

Uses ``mss`` (cross-platform, fast) to capture the entire screen, a specific
monitor, or a user-defined region. This lets users capture their game window
directly without additional hardware.

Usage::

    capturer = ScreenCapturer(region=ScreenRegion(0, 0, 1920, 1080))
    frame_bgr = capturer.grab()
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np


class ScreenCaptureError(RuntimeError):
    """Could not capture the screen or specified region."""


@dataclass(frozen=True)
class ScreenRegion:
    """A rectangular region of the screen to capture.

    All coordinates are in screen pixels. ``width`` and ``height`` must be positive.
    """

    left: int = 0
    top: int = 0
    width: int = 1920
    height: int = 1080

    def __post_init__(self) -> None:
        if self.width <= 0 or self.height <= 0:
            raise ValueError("ScreenRegion width and height must be positive")

    @property
    def right(self) -> int:
        return self.left + self.width

    @property
    def bottom(self) -> int:
        return self.top + self.height


def list_monitors() -> list[dict[str, Any]]:
    """List available monitors/screens with their bounds.

    Returns a list of dicts with keys: ``left``, ``top``, ``width``, ``height``,
    ``name`` (monitor name/index).
    """
    try:
        import mss
    except ImportError as error:
        raise ScreenCaptureError(
            "mss is required for screen capture; install with: pip install mss"
        ) from error

    with mss.mss() as sct:
        monitors: list[dict[str, Any]] = []
        for idx, mon in enumerate(sct.monitors):
            if idx == 0:
                continue  # 0 is the combined virtual screen
            monitors.append(
                {
                    "index": idx,
                    "left": mon["left"],
                    "top": mon["top"],
                    "width": mon["width"],
                    "height": mon["height"],
                    "name": mon.get("name", f"Monitor {idx}"),
                }
            )
        return monitors


def _get_cv2():
    """Lazy import of cv2 to avoid requiring it at module load time."""
    try:
        import cv2
    except ImportError as error:
        raise ScreenCaptureError(
            "OpenCV is required for screen capture color conversion; install project dependencies"
        ) from error
    return cv2


class ScreenCapturer:
    """Captures frames from a screen region using ``mss``.

    Args:
        region: The screen region to capture. If None, captures the primary
            monitor at its full resolution.
        monitor_index: 1-based monitor index. If set, overrides region with
            the full area of the specified monitor.

    Example::

        capturer = ScreenCapturer()
        frame = capturer.grab()  # Returns BGR uint8 ndarray
    """

    def __init__(
        self,
        region: ScreenRegion | None = None,
        monitor_index: int | None = None,
    ) -> None:
        try:
            import mss
        except ImportError as error:
            raise ScreenCaptureError(
                "mss is required for screen capture; install with: pip install mss"
            ) from error

        self._sct: Any = mss.mss()

        if monitor_index is not None:
            if monitor_index < 0 or monitor_index >= len(self._sct.monitors):
                raise ScreenCaptureError(
                    f"Monitor index {monitor_index} out of range "
                    f"(0-based; {len(self._sct.monitors) - 1} monitors available)"
                )
            mon = self._sct.monitors[monitor_index]
            self._region = ScreenRegion(
                left=mon["left"],
                top=mon["top"],
                width=mon["width"],
                height=mon["height"],
            )
        elif region is not None:
            self._region = region
        else:
            # Default to primary monitor (index 1 in mss, 0 is combined)
            primary = self._sct.monitors[1]
            self._region = ScreenRegion(
                left=primary["left"],
                top=primary["top"],
                width=primary["width"],
                height=primary["height"],
            )

    @property
    def region(self) -> ScreenRegion:
        return self._region

    @property
    def width(self) -> int:
        return self._region.width

    @property
    def height(self) -> int:
        return self._region.height

    def grab(self) -> np.ndarray:
        """Capture the configured screen region and return a BGR frame.

        Returns:
            ``np.ndarray`` of shape ``(height, width, 3)`` with dtype ``uint8``
            in BGR order (OpenCV-compatible).
        """
        cv2 = _get_cv2()
        mon = {
            "left": self._region.left,
            "top": self._region.top,
            "width": self._region.width,
            "height": self._region.height,
        }
        screenshot = self._sct.grab(mon)
        # mss returns BGRA; convert to BGR
        img = np.asarray(screenshot, dtype=np.uint8)
        return cv2.cvtColor(img, cv2.COLOR_BGRA2BGR)

    def isOpened(self) -> bool:
        """Match the OpenCV capture interface used by the live pipeline."""
        return self._sct is not None

    def get(self, prop_id: int) -> float:
        """Return the capture dimensions and nominal FPS for pipeline diagnostics."""
        import cv2

        if prop_id == cv2.CAP_PROP_FRAME_WIDTH:
            return float(self._region.width)
        if prop_id == cv2.CAP_PROP_FRAME_HEIGHT:
            return float(self._region.height)
        if prop_id == cv2.CAP_PROP_FPS:
            return 60.0
        return 0.0

    def read(self) -> tuple[bool, np.ndarray | None]:
        """Match ``cv2.VideoCapture.read`` for the common live pipeline loop."""
        try:
            frame = self.grab()
            return True, frame
        except Exception:
            return False, None

    def set(self, prop_id: int, value: float) -> bool:
        """Match the unsupported property-setting portion of OpenCV's interface."""
        return False

    def release(self) -> None:
        """Release the MSS screen capture context."""
        if hasattr(self, "_sct") and self._sct is not None:
            self._sct.close()
            self._sct = None

    def __enter__(self) -> ScreenCapturer:
        return self

    def __exit__(self, *args: object) -> None:
        self.release()
