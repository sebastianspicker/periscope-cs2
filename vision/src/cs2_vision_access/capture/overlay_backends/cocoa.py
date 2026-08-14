"""macOS Cocoa overlay backend."""

from __future__ import annotations

from typing import Any

import numpy as np


class CocoaOverlayBackend:
    """macOS Cocoa NSWindow with per-pixel alpha, click-through, all-Spaces."""

    def __init__(self) -> None:
        self._window: Any = None
        self._image_view: Any = None
        self._app: Any = None
        self._appkit: Any = None
        self._backing_scale: float = 1.0
        self._width: int = 0
        self._height: int = 0
        self._x: int = 0
        self._y: int = 0
        self._running = False

    @property
    def is_open(self) -> bool:
        return self._window is not None

    @property
    def position(self) -> tuple[int, int]:
        """Current window position in screen pixels."""
        return (self._x, self._y)

    def open(
        self,
        width: int,
        height: int,
        title: str = "CS2 Vision Access Overlay",
        x: int = 0,
        y: int = 0,
    ) -> None:
        import AppKit
        import Quartz

        self._appkit = AppKit
        self._width = width
        self._height = height
        self._x = int(x)
        self._y = int(y)

        app = AppKit.NSApplication.sharedApplication()
        self._app = app

        screen = AppKit.NSScreen.screens()[0]
        backing_scale = (
            screen.backingScaleFactor() if hasattr(screen, "backingScaleFactor") else 2.0
        )
        self._backing_scale = backing_scale

        rect = Quartz.CGRectMake(
            x / backing_scale, y / backing_scale, width / backing_scale, height / backing_scale
        )
        style_mask = AppKit.NSBorderlessWindowMask
        window = AppKit.NSWindow.alloc().initWithContentRect_styleMask_backing_defer_(
            rect,
            style_mask,
            AppKit.NSBackingStoreBuffered,
            False,
        )
        window.setTitle_(title)
        window.setOpaque_(False)
        window.setAlphaValue_(1.0)
        window.setBackgroundColor_(AppKit.NSColor.clearColor())
        window.setLevel_(AppKit.NSStatusWindowLevel)
        window.setIgnoresMouseEvents_(True)
        window.setCollectionBehavior_(
            AppKit.NSWindowCollectionBehaviorCanJoinAllSpaces
            | AppKit.NSWindowCollectionBehaviorFullScreenAuxiliary
        )
        window.setAcceptsMouseMovedEvents_(False)
        window.orderFrontRegardless()
        self._window = window

        # Image view
        image_view = AppKit.NSImageView.alloc().initWithFrame_(
            Quartz.CGRectMake(
                x / backing_scale, y / backing_scale, width / backing_scale, height / backing_scale
            )
        )
        image_view.setImageScaling_(AppKit.NSImageScaleAxesIndependently)
        window.contentView().addSubview_(image_view)
        self._image_view = image_view
        self._running = True

    def move(self, x: int, y: int) -> None:
        """Reposition the window without resizing (points, not backing pixels)."""
        self._x = int(x)
        self._y = int(y)
        if self._window is None:
            return
        import Quartz

        self._window.setFrameOrigin_(
            Quartz.CGPointMake(x / self._backing_scale, y / self._backing_scale)
        )

    def close(self) -> None:
        if self._window:
            self._window.orderOut_(None)
            self._window = None
        self._running = False

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        if not self._running or self._window is None:
            return
        from PIL import Image

        pil_image = Image.fromarray(frame_rgba, "RGBA")
        ns_image = _pil_to_nsimage(pil_image, self._appkit)
        self._image_view.setImage_(ns_image)

    def poll_events(self) -> bool:
        if not self._running:
            return False
        self._app.nextEventMatchingFlags_untilDate_inMode_dequeue_(
            0x7FFFFFFF,
            None,
            self._appkit.NSEventTrackingRunLoopMode,
            True,
        )
        return True


def _pil_to_nsimage(pil_image: Any, appkit: Any) -> Any:
    """Convert a PIL Image to NSImage for use with NSImageView."""
    if pil_image.mode != "RGBA":
        pil_image = pil_image.convert("RGBA")
    data = pil_image.tobytes()
    ns_data = appkit.NSData.dataWithBytes_length_(data, len(data))
    ns_image = appkit.NSImage.alloc().initWithData_(ns_data)
    return ns_image
