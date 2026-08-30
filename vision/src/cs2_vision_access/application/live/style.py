"""Argument-independent outline-style resolution for live interfaces."""

from __future__ import annotations

from cs2_vision_access.domain.outline import OutlineStyle, get_outline_preset


def resolve_outline_style(
    *,
    preset: str,
    inner_color: str | None = None,
    outer_color: str | None = None,
    inner_width: int | None = None,
    outer_width: int | None = None,
    fill_opacity: float | None = None,
    stroke_pattern: str | None = None,
    dash_period: int | None = None,
    fill_mode: str | None = None,
    halo_blur: int | None = None,
    adapt_width: bool = False,
    fixed_widths: bool = False,
    outline_kernel: str | None = None,
) -> OutlineStyle:
    """Resolve a preset with typed interface-independent overrides."""
    base = get_outline_preset(preset)
    return OutlineStyle(
        inner_color=inner_color if inner_color is not None else base.inner_color,
        outer_color=outer_color if outer_color is not None else base.outer_color,
        inner_width=inner_width if inner_width is not None else base.inner_width,
        outer_width=outer_width if outer_width is not None else base.outer_width,
        fill_opacity=fill_opacity if fill_opacity is not None else base.fill_opacity,
        scale_with_frame=False if fixed_widths else base.scale_with_frame,
        stroke_pattern=stroke_pattern if stroke_pattern is not None else base.stroke_pattern,
        dash_period_px=dash_period if dash_period is not None else base.dash_period_px,
        fill_mode=fill_mode if fill_mode is not None else base.fill_mode,
        halo_blur=halo_blur if halo_blur is not None else base.halo_blur,
        adapt_width_to_area=True if adapt_width else base.adapt_width_to_area,
        outline_kernel=outline_kernel if outline_kernel is not None else base.outline_kernel,
    )
