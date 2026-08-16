"""Win32 layered-window overlay backend."""

from __future__ import annotations

import sys
from collections.abc import Callable
from typing import Any

import numpy as np

_WIN32_CLASS_NAME = "CS2VisionOverlay"
# Keep WNDPROC callbacks alive for the process lifetime (GC would crash Win32).
_win32_wndproc_refs: list[Any] = []

# WM_HOTKEY notification for global hotkeys registered on the overlay window.
_WM_HOTKEY = 0x0312
# MOD_NOREPEAT: do not auto-repeat hotkey triggers while the key is held down.
_MOD_NOREPEAT = 0x4000

# Global hotkey VK codes → hotkey action names. Per-window hotkey ids are
# assigned 1-based in declaration order so multiple windows never collide.
_HOTKEY_VK_TO_ACTION: dict[int, str] = {
    0x1B: "quit",  # Escape
    0x20: "pause",  # Space
    0x31: "preset-1",  # 1
    0x32: "preset-2",  # 2
    0x33: "preset-3",  # 3
    0xBB: "width-up",  # =
    0xBD: "width-down",  # -
    0x54: "temporal",  # T
    0x4F: "mode-cycle",  # O
    0x46: "fill",  # F
    0x48: "diagnostics",  # H
    0x51: "quit",  # Q
}


class Win32OverlayBackend:
    """Win32 layered window with per-pixel alpha, click-through, always-on-top."""

    # Extended styles
    _WS_EX_LAYERED = 0x00080000
    _WS_EX_TRANSPARENT = 0x00000020  # click-through
    _WS_EX_TOPMOST = 0x00000008
    _WS_EX_TOOLWINDOW = 0x00000080  # hide from taskbar
    _WS_EX_NOACTIVATE = 0x08000000
    _WS_POPUP = 0x80000000
    _HWND_TOPMOST = -1
    _SWP_NOACTIVATE = 0x0010
    _SWP_SHOWWINDOW = 0x0040
    _SW_SHOWNOACTIVATE = 4
    _ULW_ALPHA = 0x00000002
    _AC_SRC_OVER = 0x00
    _AC_SRC_ALPHA = 0x01
    _DIB_RGB_COLORS = 0
    _IDC_ARROW = 32512
    _PM_REMOVE = 0x0001

    def __init__(self) -> None:
        self._window: Any = None
        self._mem_dc: Any = None
        self._dib: Any = None
        self._old_obj: Any = None
        self._bits: Any = None  # c_void_p pointing at DIB pixel storage
        self._width: int = 0
        self._height: int = 0
        self._x: int = 0
        self._y: int = 0
        self._running = False
        self._bindings: dict[str, Any] | None = None
        self._class_atom: int = 0
        self._hotkey_handler: Callable[[str], bool] | None = None
        self._hotkey_id_to_vk: dict[int, int] = {
            hotkey_id: vk for hotkey_id, vk in enumerate(_HOTKEY_VK_TO_ACTION, start=1)
        }
        self._hotkey_id_to_action: dict[int, str] = {
            hotkey_id: action
            for hotkey_id, action in enumerate(_HOTKEY_VK_TO_ACTION.values(), start=1)
        }

    @property
    def is_open(self) -> bool:
        return self._window is not None

    @property
    def position(self) -> tuple[int, int]:
        """Current window position in screen coordinates."""
        return (self._x, self._y)

    def _require_bindings(self) -> dict[str, Any]:
        """Return loaded Win32 bindings or fail for an invalid backend lifecycle."""
        bindings = self._bindings
        if bindings is None:
            raise RuntimeError("Win32 bindings are unavailable; open the overlay first")
        return bindings

    def open(
        self,
        width: int,
        height: int,
        title: str = "CS2 Vision Access Overlay",
        x: int = 0,
        y: int = 0,
    ) -> None:
        import ctypes

        if self._window is not None:
            return
        if width <= 0 or height <= 0:
            raise ValueError(f"overlay size must be positive, got {width}x{height}")

        self._width = int(width)
        self._height = int(height)
        self._x = int(x)
        self._y = int(y)

        if self._bindings is None:
            self._bindings = _load_win32()
        b = self._require_bindings()

        hinstance = b["GetModuleHandleW"](None)
        if not hinstance:
            raise RuntimeError("GetModuleHandleW failed")

        # WNDPROC must remain reachable; store on the instance and module list.
        def _wnd_proc(hwnd: int, msg: int, wparam: int, lparam: int) -> int:
            if msg == _WM_HOTKEY:
                action = self._hotkey_id_to_action.get(int(wparam))
                if action is not None and self._hotkey_handler is not None:
                    self._hotkey_handler(action)
                return 0
            return int(b["DefWindowProcW"](hwnd, msg, wparam, lparam))

        wnd_proc = b["WNDPROC"](_wnd_proc)
        self._wnd_proc = wnd_proc
        _win32_wndproc_refs.append(wnd_proc)

        wc = b["WNDCLASSW"]()
        wc.style = 0
        wc.lpfnWndProc = wnd_proc
        wc.cbClsExtra = 0
        wc.cbWndExtra = 0
        wc.hInstance = hinstance
        wc.hIcon = None
        wc.hCursor = b["LoadCursorW"](None, self._IDC_ARROW)
        wc.hbrBackground = None
        wc.lpszMenuName = None
        wc.lpszClassName = _WIN32_CLASS_NAME
        atom = b["RegisterClassW"](ctypes.byref(wc))
        # atom==0 with ERROR_CLASS_ALREADY_EXISTS (1410) is fine on re-open.
        if not atom:
            err = ctypes.get_last_error()
            if err not in (0, 1410):
                raise RuntimeError(f"RegisterClassW failed (winerror={err})")
        self._class_atom = int(atom) if atom else 0

        ex_style = (
            self._WS_EX_LAYERED
            | self._WS_EX_TRANSPARENT
            | self._WS_EX_TOPMOST
            | self._WS_EX_TOOLWINDOW
            | self._WS_EX_NOACTIVATE
        )
        hwnd = b["CreateWindowExW"](
            ex_style,
            _WIN32_CLASS_NAME,
            title,
            self._WS_POPUP,
            self._x,
            self._y,
            self._width,
            self._height,
            None,
            None,
            hinstance,
            None,
        )
        if not hwnd:
            raise RuntimeError(f"CreateWindowExW failed (winerror={ctypes.get_last_error()})")
        self._window = hwnd

        b["SetWindowPos"](
            hwnd,
            self._HWND_TOPMOST,
            self._x,
            self._y,
            self._width,
            self._height,
            self._SWP_NOACTIVATE | self._SWP_SHOWWINDOW,
        )
        b["ShowWindow"](hwnd, self._SW_SHOWNOACTIVATE)

        screen_dc = b["GetDC"](None)
        if not screen_dc:
            self.close()
            raise RuntimeError("GetDC(NULL) failed")
        try:
            self._mem_dc = b["CreateCompatibleDC"](screen_dc)
        finally:
            b["ReleaseDC"](None, screen_dc)
        if not self._mem_dc:
            self.close()
            raise RuntimeError("CreateCompatibleDC failed")

        bmi = b["BITMAPINFO"]()
        ctypes.memset(ctypes.byref(bmi), 0, ctypes.sizeof(bmi))
        bmi.bmiHeader.biSize = ctypes.sizeof(b["BITMAPINFOHEADER"])
        bmi.bmiHeader.biWidth = self._width
        bmi.bmiHeader.biHeight = -self._height  # top-down
        bmi.bmiHeader.biPlanes = 1
        bmi.bmiHeader.biBitCount = 32
        bmi.bmiHeader.biCompression = 0  # BI_RGB

        bits = ctypes.c_void_p()
        dib = b["CreateDIBSection"](
            self._mem_dc,
            ctypes.byref(bmi),
            self._DIB_RGB_COLORS,
            ctypes.byref(bits),
            None,
            0,
        )
        if not dib or not bits.value:
            self.close()
            raise RuntimeError(f"CreateDIBSection failed (winerror={ctypes.get_last_error()})")
        self._dib = dib
        self._bits = bits
        self._old_obj = b["SelectObject"](self._mem_dc, self._dib)
        self._running = True
        self._register_hotkeys()

    def move(self, x: int, y: int) -> None:
        """Reposition the window without resizing."""
        self._x = int(x)
        self._y = int(y)
        if self._window is None:
            return
        b = self._require_bindings()
        b["SetWindowPos"](
            self._window,
            self._HWND_TOPMOST,
            self._x,
            self._y,
            self._width,
            self._height,
            self._SWP_NOACTIVATE | self._SWP_SHOWWINDOW,
        )

    def set_hotkey_handler(self, handler: Callable[[str], bool] | None) -> None:
        """Install a callback invoked with hotkey action names from WM_HOTKEY."""
        self._hotkey_handler = handler

    def _register_hotkeys(self) -> None:
        """Register the fixed global hotkey set for overlay input control."""
        if self._window is None:
            return
        b = self._require_bindings()
        for hotkey_id, vk in self._hotkey_id_to_vk.items():
            b["RegisterHotKey"](self._window, hotkey_id, _MOD_NOREPEAT, vk)

    def close(self) -> None:
        if self._window is None and self._mem_dc is None and self._dib is None:
            self._running = False
            return
        self._running = False
        b = self._bindings or {}
        if self._mem_dc and self._old_obj:
            b.get("SelectObject", lambda *a: None)(self._mem_dc, self._old_obj)
        if self._dib:
            b.get("DeleteObject", lambda *a: None)(self._dib)
        if self._mem_dc:
            b.get("DeleteDC", lambda *a: None)(self._mem_dc)
        if self._window:
            for hotkey_id in self._hotkey_id_to_vk:
                b.get("UnregisterHotKey", lambda *a: None)(self._window, hotkey_id)
            b.get("DestroyWindow", lambda *a: None)(self._window)
        self._window = None
        self._mem_dc = None
        self._dib = None
        self._old_obj = None
        self._bits = None

    def show_frame(self, frame_rgba: np.ndarray) -> None:
        import ctypes

        if not self._running or self._window is None or self._bits is None:
            return
        if frame_rgba.ndim != 3 or frame_rgba.shape[2] != 4:
            raise ValueError("frame_rgba must have shape (H, W, 4)")
        h, w = frame_rgba.shape[:2]
        if w != self._width or h != self._height:
            # Resize path: recreate DIB at new size, preserving window position.
            title = "CS2 Vision Access Overlay"
            x, y = self._x, self._y
            self.close()
            self.open(w, h, title, x, y)
            if not self._running or self._bits is None:
                return

        b = self._require_bindings()

        # Windows 32-bit DIBs are BGRA; caller supplies RGBA.
        frame = np.ascontiguousarray(frame_rgba, dtype=np.uint8)
        bgra = np.empty_like(frame)
        bgra[:, :, 0] = frame[:, :, 2]
        bgra[:, :, 1] = frame[:, :, 1]
        bgra[:, :, 2] = frame[:, :, 0]
        bgra[:, :, 3] = frame[:, :, 3]
        nbytes = int(bgra.nbytes)
        ctypes.memmove(self._bits, bgra.ctypes.data, nbytes)

        pt_dst = b["POINT"](0, 0)
        size = b["SIZE"](self._width, self._height)
        pt_src = b["POINT"](0, 0)
        blend = b["BLENDFUNCTION"](
            self._AC_SRC_OVER,
            0,
            255,
            self._AC_SRC_ALPHA,
        )
        ok = b["UpdateLayeredWindow"](
            self._window,
            None,  # use screen DC
            ctypes.byref(pt_dst),
            ctypes.byref(size),
            self._mem_dc,
            ctypes.byref(pt_src),
            0,
            ctypes.byref(blend),
            self._ULW_ALPHA,
        )
        if not ok:
            # Soft-fail: keep the pipeline running if a single present fails.
            return

    def poll_events(self) -> bool:
        import ctypes

        if not self._running or self._window is None:
            return False
        b = self._require_bindings()
        msg = b["MSG"]()
        while b["PeekMessageW"](ctypes.byref(msg), None, 0, 0, self._PM_REMOVE):
            b["TranslateMessage"](ctypes.byref(msg))
            b["DispatchMessageW"](ctypes.byref(msg))
        return True


def _load_win32() -> dict[str, Any]:
    """Bind required Win32 API functions and structure types.

    ``ctypes.wintypes`` omits ``HCURSOR`` on some Python builds; fall back to
    ``HICON`` (same underlying HANDLE width).
    """
    import ctypes
    from ctypes import wintypes

    if sys.platform != "win32":  # pragma: no cover - platform guard
        raise RuntimeError("Win32 overlay bindings are only available on Windows")

    # HCURSOR is missing from ctypes.wintypes on several CPython Windows builds.
    HCURSOR = getattr(wintypes, "HCURSOR", wintypes.HICON)
    LRESULT = getattr(wintypes, "LRESULT", ctypes.c_ssize_t)
    HBITMAP = getattr(wintypes, "HBITMAP", wintypes.HANDLE)

    class POINT(ctypes.Structure):
        _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]

    class SIZE(ctypes.Structure):
        _fields_ = [("cx", wintypes.LONG), ("cy", wintypes.LONG)]

    class BLENDFUNCTION(ctypes.Structure):
        _fields_ = [
            ("BlendOp", ctypes.c_byte),
            ("BlendFlags", ctypes.c_byte),
            ("SourceConstantAlpha", ctypes.c_byte),
            ("AlphaFormat", ctypes.c_byte),
        ]

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", wintypes.DWORD),
            ("biWidth", wintypes.LONG),
            ("biHeight", wintypes.LONG),
            ("biPlanes", wintypes.WORD),
            ("biBitCount", wintypes.WORD),
            ("biCompression", wintypes.DWORD),
            ("biSizeImage", wintypes.DWORD),
            ("biXPelsPerMeter", wintypes.LONG),
            ("biYPelsPerMeter", wintypes.LONG),
            ("biClrUsed", wintypes.DWORD),
            ("biClrImportant", wintypes.DWORD),
        ]

    class BITMAPINFO(ctypes.Structure):
        _fields_ = [
            ("bmiHeader", BITMAPINFOHEADER),
            ("bmiColors", wintypes.DWORD * 1),
        ]

    class MSG(ctypes.Structure):
        _fields_ = [
            ("hwnd", wintypes.HWND),
            ("message", wintypes.UINT),
            ("wParam", wintypes.WPARAM),
            ("lParam", wintypes.LPARAM),
            ("time", wintypes.DWORD),
            ("pt", POINT),
        ]

    WNDPROC = ctypes.WINFUNCTYPE(
        LRESULT, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM
    )

    class WNDCLASSW(ctypes.Structure):
        _fields_ = [
            ("style", wintypes.UINT),
            ("lpfnWndProc", WNDPROC),
            ("cbClsExtra", ctypes.c_int),
            ("cbWndExtra", ctypes.c_int),
            ("hInstance", wintypes.HINSTANCE),
            ("hIcon", wintypes.HICON),
            ("hCursor", HCURSOR),
            ("hbrBackground", wintypes.HBRUSH),
            ("lpszMenuName", wintypes.LPCWSTR),
            ("lpszClassName", wintypes.LPCWSTR),
        ]

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
    kernel32.GetModuleHandleW.restype = wintypes.HMODULE

    user32.RegisterClassW.argtypes = [ctypes.POINTER(WNDCLASSW)]
    user32.RegisterClassW.restype = wintypes.ATOM

    user32.CreateWindowExW.argtypes = [
        wintypes.DWORD,
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.DWORD,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        wintypes.HWND,
        wintypes.HMENU,
        wintypes.HINSTANCE,
        wintypes.LPVOID,
    ]
    user32.CreateWindowExW.restype = wintypes.HWND

    user32.DestroyWindow.argtypes = [wintypes.HWND]
    user32.DestroyWindow.restype = wintypes.BOOL

    user32.DefWindowProcW.argtypes = [
        wintypes.HWND,
        wintypes.UINT,
        wintypes.WPARAM,
        wintypes.LPARAM,
    ]
    user32.DefWindowProcW.restype = LRESULT

    # IDC_* resource ids are MAKEINTRESOURCE integers, not LPCWSTR strings.
    user32.LoadCursorW.argtypes = [wintypes.HINSTANCE, ctypes.c_void_p]
    user32.LoadCursorW.restype = HCURSOR

    user32.SetWindowPos.argtypes = [
        wintypes.HWND,
        wintypes.HWND,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        wintypes.UINT,
    ]
    user32.SetWindowPos.restype = wintypes.BOOL

    user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.ShowWindow.restype = wintypes.BOOL

    user32.RegisterHotKey.argtypes = [
        wintypes.HWND,
        ctypes.c_int,
        wintypes.UINT,
        wintypes.UINT,
    ]
    user32.RegisterHotKey.restype = wintypes.BOOL

    user32.UnregisterHotKey.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.UnregisterHotKey.restype = wintypes.BOOL

    user32.GetDC.argtypes = [wintypes.HWND]
    user32.GetDC.restype = wintypes.HDC

    user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
    user32.ReleaseDC.restype = ctypes.c_int

    user32.PeekMessageW.argtypes = [
        ctypes.POINTER(MSG),
        wintypes.HWND,
        wintypes.UINT,
        wintypes.UINT,
        wintypes.UINT,
    ]
    user32.PeekMessageW.restype = wintypes.BOOL

    user32.TranslateMessage.argtypes = [ctypes.POINTER(MSG)]
    user32.TranslateMessage.restype = wintypes.BOOL

    user32.DispatchMessageW.argtypes = [ctypes.POINTER(MSG)]
    user32.DispatchMessageW.restype = LRESULT

    user32.UpdateLayeredWindow.argtypes = [
        wintypes.HWND,
        wintypes.HDC,
        ctypes.POINTER(POINT),
        ctypes.POINTER(SIZE),
        wintypes.HDC,
        ctypes.POINTER(POINT),
        wintypes.COLORREF,
        ctypes.POINTER(BLENDFUNCTION),
        wintypes.DWORD,
    ]
    user32.UpdateLayeredWindow.restype = wintypes.BOOL

    gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
    gdi32.CreateCompatibleDC.restype = wintypes.HDC

    gdi32.DeleteDC.argtypes = [wintypes.HDC]
    gdi32.DeleteDC.restype = wintypes.BOOL

    gdi32.CreateDIBSection.argtypes = [
        wintypes.HDC,
        ctypes.POINTER(BITMAPINFO),
        wintypes.UINT,
        ctypes.POINTER(ctypes.c_void_p),
        wintypes.HANDLE,
        wintypes.DWORD,
    ]
    gdi32.CreateDIBSection.restype = HBITMAP

    gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
    gdi32.SelectObject.restype = wintypes.HGDIOBJ

    gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
    gdi32.DeleteObject.restype = wintypes.BOOL

    return {
        "GetModuleHandleW": kernel32.GetModuleHandleW,
        "RegisterClassW": user32.RegisterClassW,
        "CreateWindowExW": user32.CreateWindowExW,
        "DestroyWindow": user32.DestroyWindow,
        "DefWindowProcW": user32.DefWindowProcW,
        "LoadCursorW": user32.LoadCursorW,
        "SetWindowPos": user32.SetWindowPos,
        "ShowWindow": user32.ShowWindow,
        "RegisterHotKey": user32.RegisterHotKey,
        "UnregisterHotKey": user32.UnregisterHotKey,
        "GetDC": user32.GetDC,
        "ReleaseDC": user32.ReleaseDC,
        "PeekMessageW": user32.PeekMessageW,
        "TranslateMessage": user32.TranslateMessage,
        "DispatchMessageW": user32.DispatchMessageW,
        "UpdateLayeredWindow": user32.UpdateLayeredWindow,
        "CreateCompatibleDC": gdi32.CreateCompatibleDC,
        "DeleteDC": gdi32.DeleteDC,
        "CreateDIBSection": gdi32.CreateDIBSection,
        "SelectObject": gdi32.SelectObject,
        "DeleteObject": gdi32.DeleteObject,
        "WNDCLASSW": WNDCLASSW,
        "WNDPROC": WNDPROC,
        "POINT": POINT,
        "SIZE": SIZE,
        "BLENDFUNCTION": BLENDFUNCTION,
        "BITMAPINFOHEADER": BITMAPINFOHEADER,
        "BITMAPINFO": BITMAPINFO,
        "MSG": MSG,
    }
