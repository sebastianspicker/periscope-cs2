"""Outline renderer drawing paths: solid/dashed strokes and fixed-point."""

from __future__ import annotations

import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import (
    DEFAULT_DASH_PERIOD_PX,
    DRAW_COORDINATE_SCALE,
    DRAW_COORDINATE_SHIFT,
    OutlineRenderer,
    OutlineStyle,
    pattern_polyline_segments,
)


class OutlineStyleDrawingTests(unittest.TestCase):
    def test_renderer_reports_exact_accepted_and_discarded_masks(self) -> None:
        valid = InstanceMask(
            frame_index=7,
            polygon=((1.0, 1.0), (8.0, 1.0), (4.0, 9.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        degenerate = InstanceMask(
            frame_index=7,
            polygon=((1.0, 1.0), (2.0, 2.0), (3.0, 3.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        stale = InstanceMask(
            frame_index=6,
            polygon=valid.polygon,
            confidence=0.9,
            class_id=0,
            class_name="player",
        )

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            contourArea=contour_area,
            fillPoly=lambda *_arguments, **_keywords: None,
            addWeighted=lambda _overlay, _alpha, output, _beta, _gamma: output,
            polylines=lambda *_arguments, **_keywords: None,
        )
        frame = np.zeros((720, 1280, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            _rendered, diagnostics = OutlineRenderer().render_with_diagnostics(
                frame,
                (valid, stale, degenerate),
                frame_index=7,
            )

        self.assertEqual(diagnostics.predictions_received, 3)
        self.assertEqual(diagnostics.current_predictions, 2)
        self.assertEqual(diagnostics.stale_predictions_discarded, 1)
        self.assertEqual(diagnostics.degenerate_masks_discarded, 1)
        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(diagnostics.inner_width_pixels, 3)
        self.assertEqual(diagnostics.outer_width_pixels, 7)
        self.assertEqual(diagnostics.stroke_pattern, "solid")
        self.assertEqual(diagnostics.dash_period_pixels, DEFAULT_DASH_PERIOD_PX)

    def test_solid_path_uses_closed_polylines_parity(self) -> None:
        """Solid keeps the historical closed dual-stroke polylines path."""
        mask = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (20.0, 2.0), (11.0, 18.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        draw_calls: list[dict[str, object]] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def polylines(
            _image: object,
            contours: object,
            is_closed: object,
            *_rest: object,
            **keywords: object,
        ) -> None:
            draw_calls.append(
                {
                    "contours": contours,
                    "is_closed": is_closed,
                    "shift": keywords.get("shift"),
                }
            )

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            contourArea=contour_area,
            fillPoly=lambda *_arguments, **_keywords: None,
            addWeighted=lambda _overlay, _alpha, output, _beta, _gamma: output,
            polylines=polylines,
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        style = OutlineStyle(stroke_pattern="solid", fill_opacity=0.0)
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            _rendered, diagnostics = OutlineRenderer(style).render_with_diagnostics(
                frame,
                (mask,),
                frame_index=0,
            )

        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(diagnostics.stroke_pattern, "solid")
        self.assertEqual(len(draw_calls), 2)
        for call in draw_calls:
            self.assertTrue(call["is_closed"])
            self.assertEqual(call["shift"], DRAW_COORDINATE_SHIFT)
            contours = call["contours"]
            assert isinstance(contours, list)
            self.assertEqual(len(contours), 1)
            self.assertEqual(contours[0].shape[0], 3)

    def test_dashed_path_emits_open_segments_without_temporal_state(self) -> None:
        mask = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (30.0, 2.0), (30.0, 30.0), (2.0, 30.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        draw_calls: list[dict[str, object]] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def polylines(
            _image: object,
            contours: object,
            is_closed: object,
            *_rest: object,
            **keywords: object,
        ) -> None:
            draw_calls.append(
                {
                    "contours": contours,
                    "is_closed": is_closed,
                    "shift": keywords.get("shift"),
                }
            )

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            contourArea=contour_area,
            fillPoly=lambda *_arguments, **_keywords: None,
            addWeighted=lambda _overlay, _alpha, output, _beta, _gamma: output,
            polylines=polylines,
        )
        frame = np.zeros((40, 40, 3), dtype=np.uint8)
        style = OutlineStyle(
            stroke_pattern="dashed",
            dash_period_px=10,
            fill_opacity=0.0,
        )
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            first, diagnostics = OutlineRenderer(style).render_with_diagnostics(
                frame,
                (mask,),
                frame_index=0,
            )
            second, _ = OutlineRenderer(style).render_with_diagnostics(
                frame,
                (mask,),
                frame_index=0,
            )

        self.assertEqual(diagnostics.stroke_pattern, "dashed")
        self.assertEqual(diagnostics.dash_period_pixels, 10)
        self.assertEqual(len(draw_calls), 4)  # outer+inner twice
        for call in draw_calls[:2]:
            self.assertFalse(call["is_closed"])
            contours = call["contours"]
            assert isinstance(contours, list)
            self.assertGreaterEqual(len(contours), 2)
        # Static: re-render yields identical pixel buffer (no phase/animation).
        self.assertTrue(np.array_equal(first, second))

    def test_pattern_segments_are_deterministic_and_cover_perimeter(self) -> None:
        square = (
            np.array(
                [[0.0, 0.0], [40.0, 0.0], [40.0, 40.0], [0.0, 40.0]],
                dtype=np.float64,
            )
            * DRAW_COORDINATE_SCALE
        )
        fixed = np.rint(square).astype(np.int32).reshape((-1, 1, 2))
        first = pattern_polyline_segments(fixed, "dashed", 16)
        second = pattern_polyline_segments(fixed, "dashed", 16)
        self.assertEqual(len(first), len(second))
        for left, right in zip(first, second, strict=False):
            self.assertTrue(np.array_equal(left, right))
        self.assertGreaterEqual(len(first), 2)
        dotted = pattern_polyline_segments(fixed, "dotted", 16)
        self.assertGreaterEqual(len(dotted), 2)
        # Dotted uses shorter on-runs, so typically more segments than dashed.
        self.assertGreaterEqual(len(dotted), len(first))

    def test_fractional_visible_polygon_uses_fixed_point_drawing(self) -> None:
        fractional = InstanceMask(
            frame_index=0,
            polygon=((1.05, 1.05), (1.45, 1.05), (1.05, 1.45)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        draw_calls: list[dict[str, object]] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def polylines(*_arguments: object, **keywords: object) -> None:
            draw_calls.append(keywords)

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            contourArea=contour_area,
            polylines=polylines,
        )
        style = OutlineStyle(fill_opacity=0.0)
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            _rendered, diagnostics = OutlineRenderer(style).render_with_diagnostics(
                frame,
                (fractional,),
                frame_index=0,
            )

        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(diagnostics.degenerate_masks_discarded, 0)
        self.assertEqual(len(draw_calls), 2)
        self.assertEqual(
            [call["shift"] for call in draw_calls],
            [DRAW_COORDINATE_SHIFT, DRAW_COORDINATE_SHIFT],
        )

    def test_fixed_point_collinear_polygon_is_counted_as_degenerate(self) -> None:
        quantized_collinear = InstanceMask(
            frame_index=0,
            polygon=((1.01, 1.01), (1.49, 1.49), (1.99, 2.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        with patch.dict(
            sys.modules,
            {"cv2": SimpleNamespace(contourArea=contour_area)},
        ):
            _rendered, diagnostics = OutlineRenderer().render_with_diagnostics(
                frame,
                (quantized_collinear,),
                frame_index=0,
            )

        self.assertEqual(diagnostics.contours_rendered, 0)
        self.assertEqual(diagnostics.degenerate_masks_discarded, 1)


if __name__ == "__main__":
    unittest.main()
