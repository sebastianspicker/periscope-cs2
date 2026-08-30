"""Outline-style resolution owned by the desktop GUI.

The GUI has no preferences-file input, so it resolves its selected preset and
form fields directly instead of depending on the CLI argument parser.
"""

from __future__ import annotations

from cs2_vision_access.adapters.rendering.renderer import OutlineStyle
from cs2_vision_access.application.live.style import resolve_outline_style as _resolve
from cs2_vision_access.interfaces.gui.model import GuiSettings


def resolve_outline_style(settings: GuiSettings) -> OutlineStyle:
    """Resolve GUI settings with the same preset-and-override semantics as the CLI."""
    return _resolve(
        preset=settings.outline_preset,
        inner_color=settings.inner_color,
        outer_color=settings.outer_color,
        inner_width=settings.inner_width,
        outer_width=settings.outer_width,
        fill_opacity=settings.fill_opacity,
        stroke_pattern=settings.stroke_pattern,
        dash_period=settings.dash_period,
        fill_mode=settings.fill_mode,
        halo_blur=settings.halo_blur,
        adapt_width=settings.adapt_width,
        fixed_widths=settings.fixed_widths,
        outline_kernel=settings.outline_kernel,
    )
