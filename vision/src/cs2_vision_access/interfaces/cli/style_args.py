"""Outline style argparse helpers and style resolution."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.adapters.rendering.renderer import (
    DEFAULT_HALO_BLUR,
    FILL_MODES,
    OUTLINE_KERNELS,
    OUTLINE_PRESETS,
    STROKE_PATTERNS,
    OutlineStyle,
)
from cs2_vision_access.application.configuration.prefs import (
    DEFAULT_PRESET,
    OutlinePreferences,
    load_outline_preferences,
)
from cs2_vision_access.application.configuration.prefs.resolve import (
    resolve_style_with_preferences,
)


def _add_style_arguments(
    parser: argparse.ArgumentParser,
    *,
    include_prefs: bool = True,
    include_role_config: bool = False,
) -> None:
    if include_prefs:
        parser.add_argument(
            "--prefs",
            type=Path,
            metavar="PATH",
            help=(
                "local outline preferences JSON; field precedence is "
                "preset → prefs file → explicit CLI flags"
            ),
        )
    parser.add_argument(
        "--outline-preset",
        choices=tuple(sorted(OUTLINE_PRESETS)),
        default=None,
        help=f"named dual-stroke treatment; default: {DEFAULT_PRESET}",
    )
    parser.add_argument(
        "--inner-color",
        metavar="#RRGGBB",
        help="bright stroke color override; must contrast with the outer stroke",
    )
    parser.add_argument(
        "--outer-color",
        metavar="#RRGGBB",
        help="dark stroke color override; must contrast with the inner stroke",
    )
    parser.add_argument(
        "--inner-width",
        type=int,
        help="positive inner width at 720p; overrides the preset",
    )
    parser.add_argument(
        "--outer-width",
        type=int,
        help="outer width at least two pixels wider than the inner width",
    )
    parser.add_argument(
        "--fill-opacity",
        type=float,
        help="interior tint from 0.0 to 0.35; overrides the preset",
    )
    parser.add_argument(
        "--fixed-widths",
        action="store_true",
        help="disable automatic line-width scaling above 720p",
    )
    parser.add_argument(
        "--stroke-pattern",
        choices=tuple(sorted(STROKE_PATTERNS)),
        default=None,
        help="static pattern channel: solid, dashed, or dotted; default: solid",
    )
    parser.add_argument(
        "--dash-period",
        type=int,
        metavar="PX",
        help="dash/dot period in pixels at 720p; positive integer; default: 12",
    )
    parser.add_argument(
        "--fill-mode",
        choices=tuple(sorted(FILL_MODES)),
        default=None,
        help="interior treatment: tint (hard fill) or halo (soft static glow); default: tint",
    )
    parser.add_argument(
        "--halo-blur",
        type=int,
        metavar="PX",
        help=f"halo blur kernel size in pixels (odd preferred); default: {DEFAULT_HALO_BLUR}",
    )
    parser.add_argument(
        "--adapt-width",
        action="store_true",
        help="scale stroke widths per instance from polygon area (small players thicker)",
    )
    parser.add_argument(
        "--outline-kernel",
        choices=tuple(sorted(OUTLINE_KERNELS)),
        default=None,
        help=(
            "outline draw mode: polyline (default dual-stroke), distance "
            "(OpenCV distance-transform bands), or jfa (Jump Flood bands)"
        ),
    )
    if include_role_config:
        parser.add_argument(
            "--role-config",
            type=Path,
            metavar="PATH",
            help=(
                "JSON role treatment catalog (schema v1): class-keyed styles "
                "with multi-signifier pattern/marker discrimination; "
                "see docs/examples/role-catalog.v1.json"
            ),
        )


def _resolved_style_dict(style: OutlineStyle) -> dict[str, object]:
    return {
        "inner_color": style.inner_color,
        "outer_color": style.outer_color,
        "inner_width": style.inner_width,
        "outer_width": style.outer_width,
        "fill_opacity": style.fill_opacity,
        "scale_with_frame": style.scale_with_frame,
        "stroke_pattern": style.stroke_pattern,
        "dash_period_px": style.dash_period_px,
        "fill_mode": style.fill_mode,
        "halo_blur": style.halo_blur,
        "adapt_width_to_area": style.adapt_width_to_area,
        "outline_kernel": style.outline_kernel,
    }


def _preferences_from_style_arguments(
    arguments: argparse.Namespace,
) -> OutlinePreferences:
    """Build OutlinePreferences from style CLI flags (preset alone is enough)."""
    scale_with_frame: bool | None
    scale_with_frame = False if getattr(arguments, "fixed_widths", False) else None
    adapt_width_to_area: bool | None
    adapt_width_to_area = True if getattr(arguments, "adapt_width", False) else None
    return OutlinePreferences(
        preset=(
            arguments.outline_preset if arguments.outline_preset is not None else DEFAULT_PRESET
        ),
        inner_color=arguments.inner_color,
        outer_color=arguments.outer_color,
        inner_width=arguments.inner_width,
        outer_width=arguments.outer_width,
        fill_opacity=arguments.fill_opacity,
        scale_with_frame=scale_with_frame,
        stroke_pattern=arguments.stroke_pattern,
        dash_period_px=arguments.dash_period,
        fill_mode=arguments.fill_mode,
        halo_blur=arguments.halo_blur,
        adapt_width_to_area=adapt_width_to_area,
        outline_kernel=arguments.outline_kernel,
    )


def _outline_style(arguments: argparse.Namespace) -> OutlineStyle:
    """Resolve outline style with precedence: preset → prefs file → CLI flags."""
    preferences: OutlinePreferences | None = None
    if getattr(arguments, "prefs", None) is not None:
        preferences = load_outline_preferences(arguments.prefs)

    return resolve_style_with_preferences(
        preferences,
        preset=arguments.outline_preset,
        inner_color=arguments.inner_color,
        outer_color=arguments.outer_color,
        inner_width=arguments.inner_width,
        outer_width=arguments.outer_width,
        fill_opacity=arguments.fill_opacity,
        scale_with_frame=False if arguments.fixed_widths else None,
        stroke_pattern=arguments.stroke_pattern,
        dash_period_px=arguments.dash_period,
        fill_mode=arguments.fill_mode,
        halo_blur=arguments.halo_blur,
        adapt_width_to_area=True if getattr(arguments, "adapt_width", False) else None,
        outline_kernel=arguments.outline_kernel,
    )
