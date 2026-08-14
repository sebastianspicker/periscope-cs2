"""Outline style construction, validation, presets, and width scaling."""

from __future__ import annotations

import unittest

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import (
    AREA_WIDTH_SCALE_MAX,
    AREA_WIDTH_SCALE_MIN,
    DEFAULT_DASH_PERIOD_PX,
    DEFAULT_HALO_BLUR,
    FILL_MODES,
    OUTLINE_KERNELS,
    OUTLINE_PRESET_DESCRIPTIONS,
    OUTLINE_PRESETS,
    STROKE_PATTERNS,
    OutlineStyle,
    contrast_ratio,
    current_frame_masks,
    effective_dash_period,
    effective_instance_line_widths,
    effective_line_widths,
    get_outline_preset,
    parse_hex_bgr,
)


class OutlineStyleConstructionTests(unittest.TestCase):
    def test_default_is_dual_contrast_and_static(self) -> None:
        style = OutlineStyle()

        self.assertEqual(parse_hex_bgr(style.inner_color), (0, 255, 246))
        self.assertEqual(parse_hex_bgr(style.outer_color), (16, 16, 16))
        self.assertGreaterEqual(style.outer_width, style.inner_width + 2)
        self.assertEqual(style.stroke_pattern, "solid")
        self.assertEqual(style.dash_period_px, DEFAULT_DASH_PERIOD_PX)
        self.assertEqual(STROKE_PATTERNS, frozenset({"solid", "dashed", "dotted"}))
        self.assertEqual(style.fill_mode, "tint")
        self.assertEqual(style.halo_blur, DEFAULT_HALO_BLUR)
        self.assertFalse(style.adapt_width_to_area)
        self.assertEqual(style.outline_kernel, "polyline")
        self.assertEqual(FILL_MODES, frozenset({"tint", "halo"}))
        self.assertEqual(OUTLINE_KERNELS, frozenset({"polyline", "distance", "jfa"}))

    def test_invalid_color_and_width_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "#RRGGBB"):
            OutlineStyle(inner_color="yellow")
        with self.assertRaisesRegex(ValueError, "inner_width"):
            OutlineStyle(inner_width=0)
        with self.assertRaisesRegex(ValueError, "outer_width"):
            OutlineStyle(inner_width=4, outer_width=5)
        with self.assertRaisesRegex(ValueError, "contrast ratio"):
            OutlineStyle(inner_color="#777777", outer_color="#777777")

    def test_invalid_fill_mode_kernel_and_halo_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "fill_mode"):
            OutlineStyle(fill_mode="glow")
        with self.assertRaisesRegex(ValueError, "fill_mode"):
            OutlineStyle(fill_mode="")
        with self.assertRaisesRegex(ValueError, "outline_kernel"):
            OutlineStyle(outline_kernel="flood")
        with self.assertRaisesRegex(ValueError, "outline_kernel"):
            OutlineStyle(outline_kernel="animated")
        with self.assertRaisesRegex(ValueError, "halo_blur"):
            OutlineStyle(halo_blur=0)
        with self.assertRaisesRegex(ValueError, "halo_blur"):
            OutlineStyle(halo_blur=-3)
        with self.assertRaisesRegex(ValueError, "halo_blur"):
            OutlineStyle(halo_blur=True)  # type: ignore[arg-type]
        with self.assertRaisesRegex(ValueError, "adapt_width_to_area"):
            OutlineStyle(adapt_width_to_area="yes")  # type: ignore[arg-type]
        for mode in sorted(FILL_MODES):
            with self.subTest(fill_mode=mode):
                self.assertEqual(OutlineStyle(fill_mode=mode).fill_mode, mode)
        for kernel in sorted(OUTLINE_KERNELS):
            with self.subTest(outline_kernel=kernel):
                self.assertEqual(OutlineStyle(outline_kernel=kernel).outline_kernel, kernel)

    def test_invalid_stroke_patterns_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "stroke_pattern"):
            OutlineStyle(stroke_pattern="pulse")
        with self.assertRaisesRegex(ValueError, "stroke_pattern"):
            OutlineStyle(stroke_pattern="animated")
        with self.assertRaisesRegex(ValueError, "stroke_pattern"):
            OutlineStyle(stroke_pattern="")
        with self.assertRaisesRegex(ValueError, "dash_period_px"):
            OutlineStyle(dash_period_px=0)
        with self.assertRaisesRegex(ValueError, "dash_period_px"):
            OutlineStyle(dash_period_px=-4)
        with self.assertRaisesRegex(ValueError, "dash_period_px"):
            OutlineStyle(dash_period_px=True)  # type: ignore[arg-type]
        for pattern in sorted(STROKE_PATTERNS):
            with self.subTest(pattern=pattern):
                style = OutlineStyle(stroke_pattern=pattern)
                self.assertEqual(style.stroke_pattern, pattern)

    def test_presets_are_high_contrast_and_discoverable(self) -> None:
        self.assertEqual(
            sorted(OUTLINE_PRESETS),
            ["cyan-black", "high-visibility", "maximum-visibility"],
        )
        for name in OUTLINE_PRESETS:
            with self.subTest(name=name):
                style = get_outline_preset(name)
                self.assertGreaterEqual(
                    contrast_ratio(style.inner_color, style.outer_color),
                    3.0,
                )
                self.assertEqual(style.stroke_pattern, "solid")
                self.assertIn("stroke_pattern", OUTLINE_PRESET_DESCRIPTIONS[name])

    def test_line_widths_scale_with_resolution_or_can_be_fixed(self) -> None:
        style = get_outline_preset("high-visibility")

        self.assertEqual(effective_line_widths(style, 720), (3, 7))
        self.assertEqual(effective_line_widths(style, 1080), (5, 11))
        self.assertEqual(
            effective_line_widths(
                OutlineStyle(scale_with_frame=False),
                2160,
            ),
            (3, 7),
        )

    def test_adaptive_instance_widths_match_base_when_disabled(self) -> None:
        style = OutlineStyle(adapt_width_to_area=False)
        square = ((10.0, 10.0), (50.0, 10.0), (50.0, 50.0), (10.0, 50.0))
        self.assertEqual(
            effective_instance_line_widths(style, 720, 1280, square),
            effective_line_widths(style, 720),
        )

    def test_adaptive_instance_widths_clamp_and_preserve_gap(self) -> None:
        style = OutlineStyle(adapt_width_to_area=True, scale_with_frame=False)
        # Tiny footprint thick relative strokes, clamped at max scale.
        tiny = ((0.0, 0.0), (2.0, 0.0), (1.0, 2.0))
        thick_inner, thick_outer = effective_instance_line_widths(style, 720, 1280, tiny)
        base_inner, base_outer = effective_line_widths(style, 720)
        self.assertGreaterEqual(thick_outer, thick_inner + 2)
        self.assertGreaterEqual(thick_inner, base_inner)
        # Max scale 2.5 floor(3*2.5+0.5)=8, floor(7*2.5+0.5)=18
        self.assertEqual(thick_inner, max(1, int(base_inner * AREA_WIDTH_SCALE_MAX + 0.5)))
        self.assertEqual(
            thick_outer,
            max(thick_inner + 2, int(base_outer * AREA_WIDTH_SCALE_MAX + 0.5)),
        )

        # Huge footprint (most of frame) thinner, clamped at min scale.
        huge = ((0.0, 0.0), (1279.0, 0.0), (1279.0, 719.0), (0.0, 719.0))
        thin_inner, thin_outer = effective_instance_line_widths(style, 720, 1280, huge)
        self.assertGreaterEqual(thin_outer, thin_inner + 2)
        self.assertEqual(thin_inner, max(1, int(base_inner * AREA_WIDTH_SCALE_MIN + 0.5)))
        self.assertEqual(
            thin_outer,
            max(thin_inner + 2, int(base_outer * AREA_WIDTH_SCALE_MIN + 0.5)),
        )
        with self.assertRaisesRegex(ValueError, "frame_width"):
            effective_instance_line_widths(style, 720, 0, tiny)

    def test_dash_period_scales_with_resolution_like_widths(self) -> None:
        style = OutlineStyle(dash_period_px=12)
        self.assertEqual(effective_dash_period(style, 720), 12)
        self.assertEqual(effective_dash_period(style, 1080), 18)
        self.assertEqual(effective_dash_period(style, 1440), 24)
        self.assertEqual(
            effective_dash_period(
                OutlineStyle(dash_period_px=12, scale_with_frame=False),
                2160,
            ),
            12,
        )
        with self.assertRaisesRegex(ValueError, "frame_height"):
            effective_dash_period(style, 0)

    def test_stale_predictions_are_discarded(self) -> None:
        current = InstanceMask(
            frame_index=7,
            polygon=((1.0, 1.0), (5.0, 1.0), (3.0, 6.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        stale = InstanceMask(
            frame_index=6,
            polygon=((1.0, 1.0), (5.0, 1.0), (3.0, 6.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )

        self.assertEqual(current_frame_masks((stale, current), 7), (current,))


if __name__ == "__main__":
    unittest.main()
