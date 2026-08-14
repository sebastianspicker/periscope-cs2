"""Display management for the live pipeline — window, overlay, diagnostics, hotkeys.

Encapsulates all OpenCV window and overlay-window concerns so the main
pipeline loop focuses on capture → infer → render.
"""

from __future__ import annotations

import time
from typing import TYPE_CHECKING, Any

import numpy as np

from cs2_vision_access.capture.hotkeys import (
    LiveControlState,
    apply_hotkey_action,
    format_diagnostic_text,
    handle_live_key,
)
from cs2_vision_access.capture.overlay import OverlayWindow

if TYPE_CHECKING:
    from collections.abc import Callable


def _make_hotkey_handler(state: LiveControlState) -> Callable[[str], bool]:
    """Build an overlay hotkey handler that applies actions to ``state``."""

    def _handle(action: str) -> bool:
        return apply_hotkey_action(action, state)

    return _handle


class DisplayManager:
    """Manages an OpenCV preview window and/or transparent overlay window.

    Args:
        cv2_module: The ``cv2`` module (injected to keep lazy import).
        window_name: Title for the OpenCV preview window.
        display_width: Scaled display width for the OpenCV window.
        display_height: Scaled display height for the OpenCV window.
        overlay_window: Optional transparent always-on-top overlay.
        enable_hotkeys: Whether to process hotkeys in the display loop.
        control_state: Mutable control state modified by hotkeys.
        on_style_changed: Callback invoked when hotkeys change the style or mode.
    """

    def __init__(
        self,
        cv2_module: Any,
        window_name: str = "CS2 Vision Access - Live",
        display_width: int = 0,
        display_height: int = 0,
        overlay_window: OverlayWindow | None = None,
        enable_hotkeys: bool = True,
        control_state: LiveControlState | None = None,
        on_style_changed: Callable[[], None] | None = None,
    ) -> None:
        self._cv2 = cv2_module
        self._window_name = window_name
        self._display_width = display_width
        self._display_height = display_height
        self._overlay = overlay_window
        self._enable_hotkeys = enable_hotkeys
        self._control_state = control_state or LiveControlState()
        self._on_style_changed = on_style_changed
        self._last_display_frame: np.ndarray | None = None
        self._quit_requested = False

    @property
    def control_state(self) -> LiveControlState:
        return self._control_state

    @property
    def quit_requested(self) -> bool:
        return self._quit_requested

    # ------------------------------------------------------------------
    # Window lifecycle
    # ------------------------------------------------------------------

    def open(self, actual_width: int, actual_height: int) -> None:
        """Create the preview window or open the overlay."""
        if self._overlay is not None:
            self._overlay.open(actual_width, actual_height)
            if self._enable_hotkeys:
                handler = _make_hotkey_handler(self._control_state)

                def _overlay_hotkey(action: str) -> bool:
                    if handler(action):
                        self._quit_requested = True
                        return True
                    return False

                self._overlay.set_hotkey_handler(_overlay_hotkey)
        elif self._display_width > 0 and self._display_height > 0:
            cv2 = self._cv2
            cv2.namedWindow(self._window_name, cv2.WINDOW_NORMAL)
            cv2.resizeWindow(self._window_name, self._display_width, self._display_height)

    def close(self) -> None:
        """Destroy the preview window and overlay."""
        if self._overlay is not None:
            self._overlay.close()
        elif self._display_width > 0 and self._display_height > 0:
            self._cv2.destroyWindow(self._window_name)

    def set_hotkey_handler(self, handler: Callable[[str], bool] | None) -> None:
        """Delegate hotkey handling to the overlay window."""
        if self._overlay is not None:
            self._overlay.set_hotkey_handler(handler)

    def notify_hotkey(self, action: str) -> bool:
        """Apply a hotkey action to the control state. Returns True if quit."""
        if apply_hotkey_action(action, self._control_state):
            self._quit_requested = True
            return True
        return False

    # ------------------------------------------------------------------
    # Per-frame display
    # ------------------------------------------------------------------

    def poll_events(self) -> bool:
        """Process overlay events. Returns False if the user closed the window or requested quit."""
        if self._quit_requested:
            return False
        if self._overlay is not None and not self._overlay.poll_events():
            self._quit_requested = True
            return False
        return True

    def show_paused(self, elapsed_adjust: Callable[[float], None] | None = None) -> bool:
        """Display the paused indicator and poll for unpause/quit.

        Args:
            elapsed_adjust: Optional callback to adjust elapsed timers by pause duration.

        Returns:
            True to continue, False if quit was requested.
        """
        cv2 = self._cv2
        state = self._control_state

        if self._last_display_frame is not None and self._overlay is None:
            paused_frame = self._last_display_frame.copy()
            cv2.putText(
                paused_frame,
                "PAUSED",
                (10, max(30, paused_frame.shape[0] - 15)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (0, 255, 255),
                2,
            )
            cv2.imshow(self._window_name, paused_frame)

        if self._overlay is None:
            key = cv2.waitKey(100) & 0xFF
            if self._enable_hotkeys and handle_live_key(key, state, cv2=cv2):
                self._quit_requested = True
                return False
            self._notify_style_if_changed()

        return True

    def show_frame(
        self,
        rendered: np.ndarray,
        frame_index: int,
        inference_ms: list[float],
        predictions_count: int,
        started: float,
    ) -> bool:
        """Display a rendered frame. Returns False if the user requested quit."""
        cv2 = self._cv2
        state = self._control_state

        if self._overlay is not None:
            if rendered.ndim == 3 and rendered.shape[2] == 4:
                self._overlay.show_frame(rendered)
            return True

        if self._display_width <= 0 or self._display_height <= 0:
            return True

        display_frame = cv2.resize(rendered, (self._display_width, self._display_height))
        if display_frame.ndim == 3 and display_frame.shape[2] == 4:
            display_frame = cv2.cvtColor(display_frame, cv2.COLOR_RGBA2BGR)

        elapsed = time.perf_counter() - started
        effective_fps = frame_index / elapsed if elapsed > 0 else 0.0

        if state.show_diagnostics:
            diagnostic_text = format_diagnostic_text(
                state,
                frame_index,
                inference_ms,
                effective_fps,
                predictions_count,
            )
            for line_index, line in enumerate(diagnostic_text.split("\n")):
                cv2.putText(
                    display_frame,
                    line,
                    (10, 20 + line_index * 18),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.45,
                    (255, 255, 255),
                    1,
                )

        self._last_display_frame = display_frame
        cv2.imshow(self._window_name, display_frame)
        key = cv2.waitKey(1) & 0xFF

        if self._enable_hotkeys and handle_live_key(key, state, cv2=cv2):
            self._quit_requested = True
            return False

        self._notify_style_if_changed()
        return True

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    _last_output_mode: str = ""
    _last_alpha_fill: bool = False

    def _notify_style_if_changed(self) -> None:
        """Detect hotkey-driven mode/fill changes and notify the pipeline."""
        state = self._control_state
        if state.output_mode != self._last_output_mode or state.alpha_fill != self._last_alpha_fill:
            self._last_output_mode = state.output_mode
            self._last_alpha_fill = state.alpha_fill
            if self._on_style_changed is not None:
                self._on_style_changed()
