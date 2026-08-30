"""Real-time keyboard controls for the live overlay pipeline.

Provides an interactive controls system that lets users adjust outline
parameters (preset, width, mode, etc.) while the pipeline is running,
without restarting.

Controls (press in the display window):
  Q / Esc  — Quit
  Space    — Pause/resume
  1        — high-visibility preset
  2        — maximum-visibility preset
  3        — cyan-black preset
  + / =    — Increase outline width (inner + outer)
  - / _    — Decrease outline width
  T        — Toggle temporal stability filter
  O        — Cycle output mode (overlay → alpha → green → overlay)
  F        — Toggle alpha fill on/off
  H        — Toggle headless-friendly info display
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from cs2_vision_access.adapters.rendering.renderer import (
    OutlineStyle,
    get_outline_preset,
)

# Output mode cycle order
_OUTPUT_MODES = ("overlay", "alpha", "green")

# Named actions that can be driven by keyboard shortcuts or external handlers.
HOTKEY_ACTIONS: frozenset[str] = frozenset(
    {
        "quit",
        "pause",
        "preset-1",
        "preset-2",
        "preset-3",
        "width-up",
        "width-down",
        "temporal",
        "mode-cycle",
        "fill",
        "diagnostics",
    }
)

# cv2.waitKey() key codes → hotkey action names (keep in sync with HOTKEY_ACTIONS).
_KEY_ACTIONS: dict[int, str] = {
    27: "quit",
    ord("q"): "quit",
    ord("Q"): "quit",
    ord(" "): "pause",
    ord("1"): "preset-1",
    ord("2"): "preset-2",
    ord("3"): "preset-3",
    ord("="): "width-up",
    ord("+"): "width-up",
    ord("-"): "width-down",
    ord("_"): "width-down",
    ord("t"): "temporal",
    ord("T"): "temporal",
    ord("o"): "mode-cycle",
    ord("O"): "mode-cycle",
    ord("f"): "fill",
    ord("F"): "fill",
    ord("h"): "diagnostics",
    ord("H"): "diagnostics",
}


# Initial state shared between the key handler and the rendering loop
@dataclass
class LiveControlState:
    """Mutable state that can be adjusted via hotkeys during live mode."""

    paused: bool = False
    current_preset: str = "maximum-visibility"
    output_mode: str = "overlay"
    alpha_fill: bool = False
    temporal_enabled: bool = False
    width_offset: int = 0  # +/- adjustment to base widths
    show_diagnostics: bool = True
    style_override: OutlineStyle | None = None

    def apply_style(self, base_style: OutlineStyle) -> OutlineStyle:
        """Return an OutlineStyle with current hotkey adjustments applied."""
        if self.style_override is not None:
            return self.style_override

        w = self.width_offset
        return OutlineStyle(
            inner_color=base_style.inner_color,
            outer_color=base_style.outer_color,
            inner_width=max(1, base_style.inner_width + w),
            outer_width=max(3, base_style.outer_width + w),
            fill_opacity=base_style.fill_opacity,
            scale_with_frame=base_style.scale_with_frame,
            stroke_pattern=base_style.stroke_pattern,
            dash_period_px=base_style.dash_period_px,
            fill_mode=base_style.fill_mode,
            halo_blur=base_style.halo_blur,
            adapt_width_to_area=base_style.adapt_width_to_area,
            outline_kernel=base_style.outline_kernel,
        )


def apply_hotkey_action(
    action: str,
    state: LiveControlState,
    *,
    printer: Callable[[str], None] = print,
) -> bool:
    """Apply a single named hotkey action to ``state``.

    Args:
        action: One of :data:`HOTKEY_ACTIONS`.
        state: Mutable ``LiveControlState`` modified in-place.
        printer: Callable used to report status changes.

    Returns:
        ``True`` if the application should quit, ``False`` to continue.
    """
    if action == "quit":
        return True

    if action == "pause":
        state.paused = not state.paused
        printer(f"[live] {'PAUSED' if state.paused else 'RESUMED'}")
        return False

    if action == "preset-1":
        state.current_preset = "high-visibility"
        state.style_override = get_outline_preset(state.current_preset)
        printer("[live] Preset: high-visibility")
        return False

    if action == "preset-2":
        state.current_preset = "maximum-visibility"
        state.style_override = get_outline_preset(state.current_preset)
        printer("[live] Preset: maximum-visibility")
        return False

    if action == "preset-3":
        state.current_preset = "cyan-black"
        state.style_override = get_outline_preset(state.current_preset)
        printer("[live] Preset: cyan-black")
        return False

    if action == "width-up":
        state.width_offset = min(20, state.width_offset + 1)
        state.style_override = None
        printer(f"[live] Width offset: +{state.width_offset}")
        return False

    if action == "width-down":
        state.width_offset = max(-5, state.width_offset - 1)
        state.style_override = None
        printer(f"[live] Width offset: {state.width_offset}")
        return False

    if action == "temporal":
        state.temporal_enabled = not state.temporal_enabled
        printer(f"[live] Temporal stability: {'ON' if state.temporal_enabled else 'OFF'}")
        return False

    if action == "mode-cycle":
        current_index = _OUTPUT_MODES.index(state.output_mode)
        state.output_mode = _OUTPUT_MODES[(current_index + 1) % len(_OUTPUT_MODES)]
        printer(f"[live] Output mode: {state.output_mode}")
        return False

    if action == "fill":
        state.alpha_fill = not state.alpha_fill
        printer(f"[live] Alpha fill: {'ON' if state.alpha_fill else 'OFF'}")
        return False

    if action == "diagnostics":
        state.show_diagnostics = not state.show_diagnostics
        printer(f"[live] Diagnostics overlay: {'ON' if state.show_diagnostics else 'OFF'}")
        return False

    return False


def handle_live_key(
    key: int,
    state: LiveControlState,
    *,
    cv2: Any,
    window_name: str = "",  # noqa: ARG001
) -> bool:
    """Process a keypress from the live display window.

    Args:
        key: The key code from ``cv2.waitKey() & 0xFF``.
        state: Mutable ``LiveControlState`` modified in-place.

    Returns:
        ``True`` if the application should quit, ``False`` to continue.
    """
    action = _KEY_ACTIONS.get(key)
    if action is None:
        return False
    return apply_hotkey_action(action, state)


def format_diagnostic_text(
    state: LiveControlState,
    frame_index: int,
    inference_ms: list[float],
    fps: float,
    predictions_count: int,
) -> str:
    """Generate a multi-line diagnostic string for overlay on the display."""
    avg_inference = sum(inference_ms[-30:]) / max(1, len(inference_ms[-30:]))
    lines = [
        f"Frame: {frame_index}  FPS: {fps:.1f}",
        f"Inference: {avg_inference:.1f}ms  Predictions: {predictions_count}",
        f"Preset: {state.current_preset}  Width offset: {state.width_offset:+d}",
        f"Mode: {state.output_mode}{' FILL' if state.alpha_fill else ''}",
        f"Temporal: {'ON' if state.temporal_enabled else 'OFF'}",
        "",
        "[1-3] Preset  [+/-] Width  [T] Temporal  [O] Mode",
        "[F] Fill  [H] Info  [Space] Pause  [Q] Quit",
    ]
    return "\n".join(lines)
