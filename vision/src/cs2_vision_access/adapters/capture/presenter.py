"""Fullscreen presenter backends for the second-device composite.

The primary contour topology renders on a second Windows machine: the gaming
PC's HDMI output is captured, segmented, and composited, and the result is
shown fullscreen. This module wraps that presentation surface.

``Win32FullscreenPresenter`` is the real backend: borderless popup window,
per-pixel alpha via ``UpdateLayeredWindow``, topmost, non-activating, optional
capture exclusion (streamproof). It never touches the cursor; Windows keeps
drawing its own pointer over the window. The pure geometry checks -
native-resolution blitting, capture/present size agreement, live in
``resolve_present_size`` so they can be tested without a display.
"""

from __future__ import annotations

import ctypes
import sys
from typing import Any

import numpy as np

from cs2_vision_access.adapters.capture.presentation import resolve_present_size

_presenter_wndproc: Any = None


class PresenterUnavailable(RuntimeError):
    """No presenter backend for this platform."""


def _last_error() -> int:
    """LastError from ctypes, masked to a Win32 DWORD."""
    return int(ctypes.get_last_error()) & 0xFFFF_FFFF  # type: ignore[attr-defined]


def _require_win32() -> dict[str, Any]:
    """Load the Win32 bindings shared with the overlay backend."""
    from cs2_vision_access.adapters.capture.overlay_backends.win32 import _load_win32

    bindings = _load_win32()
    if bindings is None:
        raise PresenterUnavailable("win32 bindings unavailable")
    return bindings


class Win32FullscreenPresenter:
    """Present composited frames fullscreen, native pixels, no chrome."""

    def __init__(
        self,
        title: str = "Periscope Contour",
        *,
        streamproof: bool = False,
    ) -> None:
        self._title = title
        self._streamproof = streamproof
        self._window: int | None = None
        self._mem_dc: int | None = None
        self._dib: int | None = None
        self._bits: int | None = None
        self._old_obj: int | None = None
        self._width = 0
        self._height = 0
        self._x = 0
        self._y = 0
        self._bits_view: np.ndarray | None = None
        self._bindings: dict[str, Any] | None = None
        self._streamproof_mode: str = "inactive"

    def open(self, width: int, height: int, *, x: int = 0, y: int = 0) -> None:
        """Create the fullscreen window for the target monitor."""
        b = _require_win32()
        self._bindings = b
        self._width = int(width)
        self._height = int(height)
        self._x = int(x)
        self._y = int(y)
        hinstance = b["GetModuleHandleW"](None)
        global _presenter_wndproc
        if _presenter_wndproc is None:
            # One WNDPROC for the class, alive for the process: the class keeps the
            # first registration, so a per-instance callback could be freed under it.
            _presenter_wndproc = b["WNDPROC"](
                lambda hwnd, msg, wparam, lparam: int(
                    b["DefWindowProcW"](hwnd, msg, wparam, lparam)
                )
            )
            wc = b["WNDCLASSW"]()
            wc.lpfnWndProc = _presenter_wndproc
            wc.lpszClassName = "PeriscopeContourPresenter"
            wc.hInstance = hinstance
            b["RegisterClassW"](ctypes.byref(wc))

        # WS_POPUP: no chrome at all. SW_SHOWNOACTIVATE keeps the compositor
        # from stealing focus when it comes up alongside the game.
        ex_style = b["WS_EX_TOPMOST"] | b["WS_EX_LAYERED"] | b["WS_EX_NOACTIVATE"]
        hwnd = b["CreateWindowExW"](
            ex_style,
            "PeriscopeContourPresenter",
            self._title,
            b["WS_POPUP"],
            x,
            y,
            self._width,
            self._height,
            None,
            None,
            hinstance,
            None,
        )
        if not hwnd:
            raise RuntimeError(f"CreateWindowExW failed (winerror={_last_error()})")
        self._window = hwnd

        screen_dc = b["GetDC"](None)
        try:
            self._mem_dc = b["CreateCompatibleDC"](screen_dc)
        finally:
            b["ReleaseDC"](None, screen_dc)

        bmi = b["BITMAPINFO"]()
        bmi.bmiHeader.biSize = ctypes.sizeof(b["BITMAPINFOHEADER"])
        bmi.bmiHeader.biWidth = self._width
        bmi.bmiHeader.biHeight = -self._height  # top-down
        bmi.bmiHeader.biPlanes = 1
        bmi.bmiHeader.biBitCount = 32
        bmi.bmiHeader.biCompression = 0
        bits = ctypes.c_void_p()
        self._dib = b["CreateDIBSection"](
            self._mem_dc, ctypes.byref(bmi), b["DIB_RGB_COLORS"], ctypes.byref(bits), None, 0
        )
        self._bits = bits.value
        self._old_obj = b["SelectObject"](self._mem_dc, self._dib)
        if self._bits:
            # Persistent BGRA view over the DIB so show_frame converts in place.
            self._bits_view = np.ctypeslib.as_array(
                (ctypes.c_uint8 * (self._width * self._height * 4)).from_address(self._bits)
            ).reshape(self._height, self._width, 4)

        b["SetWindowPos"](
            hwnd,
            b["HWND_TOPMOST"],
            x,
            y,
            self._width,
            self._height,
            b["SWP_NOACTIVATE"] | b["SWP_SHOWWINDOW"],
        )
        if self._streamproof:
            self._apply_display_affinity()

    def _apply_display_affinity(self) -> None:
        """Exclude the presenter window from capture (OBS / game capture).

        Ledger E10 fallback ladder: prefer ``WDA_EXCLUDEFROMCAPTURE`` (Win10
        2004+, window invisible to captures but visible on the desktop); when
        the call fails, older Windows, RDP sessions, some drivers, retry
        with ``WDA_MONITOR`` (window captures as black on device 2 instead of
        being excluded). The applied affinity is recorded in
        ``streamproof_mode`` so operators can see which tier engaged; a total
        failure leaves the mode as ``"none"`` and does not abort the open.
        """
        b = self._bindings
        if b is None or self._window is None:
            return
        if "SetWindowDisplayAffinity" not in b:
            self._streamproof_mode = "unsupported"
            return
        wda_excludefromcapture = 0x11  # WDA_EXCLUDEFROMCAPTURE, Win10 2004+
        wda_monitor = 0x1  # WDA_MONITOR, Win7+: captures show black
        applied: str | None = None
        if b["SetWindowDisplayAffinity"](self._window, wda_excludefromcapture):
            applied = "WDA_EXCLUDEFROMCAPTURE"
        if applied is None and b["SetWindowDisplayAffinity"](self._window, wda_monitor):
            applied = "WDA_MONITOR"
        self._streamproof_mode = applied if applied is not None else "none"

    @property
    def streamproof_mode(self) -> str:
        """Which capture-exclusion tier engaged: a mode name, ``"none"``, or
        ``"unsupported"`` (no ``SetWindowDisplayAffinity`` binding). Only
        meaningful after ``open`` and only when ``streamproof`` was set."""
        return self._streamproof_mode

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        """Blit a native-resolution RGBA frame onto the presenter surface.

        The geometry check is the pipeline's 1:1 contract: a composited frame
        that does not match the presenter resolution is a configuration error,
        not something to scale over.
        """
        b = self._bindings
        if self._window is None or self._bits_view is None or b is None:
            return
        h, w = frame_rgba.shape[:2]
        resolve_present_size(w, h, self._width, self._height)
        if frame_rgba.ndim != 3 or frame_rgba.shape[2] != 4:
            raise ValueError("presenter expects RGBA frames of shape (H, W, 4)")
        import cv2

        frame = np.ascontiguousarray(frame_rgba, dtype=np.uint8)
        cv2.cvtColor(frame, cv2.COLOR_RGBA2BGRA, dst=self._bits_view)

        pt_dst = b["POINT"](self._x, self._y)
        size = b["SIZE"](self._width, self._height)
        pt_src = b["POINT"](0, 0)
        blend = b["BLENDFUNCTION"](0, 0, 255, 1)  # AC_SRC_OVER, alpha
        b["UpdateLayeredWindow"](
            self._window,
            None,
            ctypes.byref(pt_dst),
            ctypes.byref(size),
            self._mem_dc,
            ctypes.byref(pt_src),
            0,
            ctypes.byref(blend),
            b["ULW_ALPHA"],
        )

    def poll_events(self) -> bool:
        """Pump messages. Returns False when the window is gone."""
        if self._window is None or self._bindings is None:
            return False
        msg = self._bindings["MSG"]()
        while self._bindings["PeekMessageW"](
            ctypes.byref(msg), None, 0, 0, self._bindings["PM_REMOVE"]
        ):
            self._bindings["TranslateMessageW"](ctypes.byref(msg))
            self._bindings["DispatchMessageW"](ctypes.byref(msg))
        return True

    def close(self) -> None:
        """Release the window and its GDI objects."""
        b = self._bindings or {}
        self._bits_view = None
        if self._mem_dc and self._old_obj:
            b.get("SelectObject", lambda *a: None)(self._mem_dc, self._old_obj)
        if self._dib:
            b.get("DeleteObject", lambda *a: None)(self._dib)
        if self._mem_dc:
            b.get("DeleteDC", lambda *a: None)(self._mem_dc)
        if self._window is not None:
            b.get("DestroyWindow", lambda *a: None)(self._window)
        self._window = None
        self._mem_dc = None
        self._dib = None
        self._bits = None
        self._old_obj = None


def create_fullscreen_presenter(
    width: int,
    height: int,
    *,
    title: str = "Periscope Contour",
    streamproof: bool = False,
) -> Any:
    """Return the platform presenter, raising on unsupported platforms."""
    if sys.platform != "win32":
        raise PresenterUnavailable(
            f"fullscreen composite is Windows-only; {sys.platform} hosts can "
            "use --output-mode overlay on the gaming machine instead"
        )
    presenter = Win32FullscreenPresenter(title=title, streamproof=streamproof)
    presenter.open(width, height)
    return presenter
