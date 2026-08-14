"""Outline renderer fill modes and outline kernels (halo, distance, jfa)."""

from __future__ import annotations

import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import (
    DRAW_COORDINATE_SHIFT,
    OutlineRenderer,
    OutlineStyle,
)


class OutlineStyleKernelTests(unittest.TestCase):
    def test_halo_and_distance_defaults_do_not_change_solid_polyline_path(self) -> None:
        """Default tint+polyline path stays closed dual-stroke; upgrades are opt-in."""
        mask = InstanceMask(
            frame_index=0,
            polygon=((4.0, 4.0), (28.0, 4.0), (16.0, 24.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        draw_calls: list[dict[str, object]] = []
        fill_calls: list[str] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def fill_poly(*_args: object, **_kwargs: object) -> None:
            fill_calls.append("fillPoly")

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
            fillPoly=fill_poly,
            addWeighted=lambda _overlay, _alpha, output, _beta, _gamma: output,
            polylines=polylines,
            # Halo/distance APIs must not be required on the solid default path.
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        default_style = OutlineStyle()
        self.assertEqual(default_style.fill_mode, "tint")
        self.assertEqual(default_style.outline_kernel, "polyline")
        self.assertFalse(default_style.adapt_width_to_area)

        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            _rendered, diagnostics = OutlineRenderer(default_style).render_with_diagnostics(
                frame,
                (mask,),
                frame_index=0,
            )

        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(len(fill_calls), 1)  # tint fillPoly only
        self.assertEqual(len(draw_calls), 2)  # outer + inner polylines
        for call in draw_calls:
            self.assertTrue(call["is_closed"])
            self.assertEqual(call["shift"], DRAW_COORDINATE_SHIFT)

        # Explicit opt-in styles are accepted without changing defaults of presets.
        halo = OutlineStyle(fill_mode="halo", fill_opacity=0.1, halo_blur=7)
        self.assertEqual(halo.fill_mode, "halo")
        distance = OutlineStyle(outline_kernel="distance")
        self.assertEqual(distance.outline_kernel, "distance")
        adaptive = OutlineStyle(adapt_width_to_area=True)
        self.assertTrue(adaptive.adapt_width_to_area)

    def test_halo_path_uses_dilate_blur_not_tint_only(self) -> None:
        mask = InstanceMask(
            frame_index=0,
            polygon=((4.0, 4.0), (28.0, 4.0), (16.0, 24.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        ops: list[str] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def fill_poly(image: object, *_args: object, **_kwargs: object) -> None:
            ops.append("fillPoly")
            if isinstance(image, np.ndarray) and image.ndim == 2:
                image[:] = 255

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            MORPH_ELLIPSE=2,
            contourArea=contour_area,
            fillPoly=fill_poly,
            getStructuringElement=lambda *_a, **_k: np.ones((3, 3), dtype=np.uint8),
            dilate=lambda src, *_a, **_k: ops.append("dilate") or src,
            GaussianBlur=lambda src, *_a, **_k: ops.append("GaussianBlur") or src,
            polylines=lambda *_a, **_k: ops.append("polylines"),
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        style = OutlineStyle(fill_mode="halo", fill_opacity=0.12, halo_blur=5)
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            OutlineRenderer(style).render_with_diagnostics(frame, (mask,), frame_index=0)

        self.assertIn("dilate", ops)
        self.assertIn("GaussianBlur", ops)
        self.assertIn("polylines", ops)  # dual-stroke still on top
        self.assertNotIn("addWeighted", ops)

    def test_distance_kernel_is_static_and_uses_distance_transform(self) -> None:
        mask = InstanceMask(
            frame_index=0,
            polygon=((6.0, 6.0), (26.0, 6.0), (26.0, 26.0), (6.0, 26.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        ops: list[str] = []

        def contour_area(contour: np.ndarray) -> float:
            points = contour.reshape((-1, 2)).astype(np.float64)
            shifted = np.roll(points, -1, axis=0)
            return abs(
                float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0)
            )

        def distance_transform(src: np.ndarray, *_a: object, **_k: object) -> np.ndarray:
            ops.append("distanceTransform")
            # Simple deterministic surrogate: distance ≈ 0 on zeros, large on ones.
            return (src > 0).astype(np.float32) * 10.0

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            DIST_L2=2,
            contourArea=contour_area,
            fillPoly=lambda image, *_a, **_k: (
                ops.append("fillPoly")
                or (image.__setitem__(slice(None), 255) if image.ndim == 2 else None)
            ),
            addWeighted=lambda _o, _a, output, _b, _g: output,
            bitwise_not=lambda src: 255 - src,
            distanceTransform=distance_transform,
            polylines=lambda *_a, **_k: ops.append("polylines"),
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        style = OutlineStyle(
            outline_kernel="distance",
            fill_opacity=0.0,
            scale_with_frame=False,
        )
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            first, _ = OutlineRenderer(style).render_with_diagnostics(frame, (mask,), frame_index=0)
            second, _ = OutlineRenderer(style).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )

        self.assertIn("distanceTransform", ops)
        self.assertNotIn("polylines", ops)
        self.assertTrue(np.array_equal(first, second))

    def test_jfa_kernel_differs_from_polyline_and_is_deterministic(self) -> None:
        """JFA bands are non-empty, differ from polyline, and double-render equal."""
        mask = InstanceMask(
            frame_index=0,
            polygon=((8.0, 8.0), (24.0, 8.0), (24.0, 24.0), (8.0, 24.0)),
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

        def fill_poly(
            image: np.ndarray,
            contours: object,
            color: object,
            *_args: object,
            **keywords: object,
        ) -> None:
            shift = int(keywords.get("shift") or 0)
            scale = float(1 << shift) if shift else 1.0
            for contour in contours:  # type: ignore[union-attr]
                pts = np.asarray(contour).reshape((-1, 2)).astype(np.float64) / scale
                x0 = max(0, int(np.floor(pts[:, 0].min())))
                y0 = max(0, int(np.floor(pts[:, 1].min())))
                x1 = min(image.shape[1], int(np.ceil(pts[:, 0].max())) + 1)
                y1 = min(image.shape[0], int(np.ceil(pts[:, 1].max())) + 1)
                if image.ndim == 2:
                    image[y0:y1, x0:x1] = 255 if not isinstance(color, tuple) else color
                else:
                    image[y0:y1, x0:x1] = color

        def polylines(
            image: np.ndarray,
            contours: object,
            _is_closed: object,
            color: object,
            thickness: object,
            *_rest: object,
            **keywords: object,
        ) -> None:
            # Deterministic stroke surrogate: paint contour vertex neighborhoods.
            shift = int(keywords.get("shift") or 0)
            scale = float(1 << shift) if shift else 1.0
            t = max(1, int(thickness) // 2)
            for contour in contours:  # type: ignore[union-attr]
                pts = np.asarray(contour).reshape((-1, 2)).astype(np.float64) / scale
                for x, y in pts:
                    cx, cy = int(round(x)), int(round(y))
                    y0, y1 = max(0, cy - t), min(image.shape[0], cy + t + 1)
                    x0, x1 = max(0, cx - t), min(image.shape[1], cx + t + 1)
                    image[y0:y1, x0:x1] = color

        fake_cv2 = SimpleNamespace(
            LINE_AA=16,
            contourArea=contour_area,
            fillPoly=fill_poly,
            addWeighted=lambda _o, _a, output, _b, _g: output,
            polylines=polylines,
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        jfa_style = OutlineStyle(
            outline_kernel="jfa",
            fill_opacity=0.0,
            scale_with_frame=False,
            inner_width=3,
            outer_width=7,
        )
        poly_style = OutlineStyle(
            outline_kernel="polyline",
            fill_opacity=0.0,
            scale_with_frame=False,
            inner_width=3,
            outer_width=7,
        )
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            jfa_first, jfa_diag = OutlineRenderer(jfa_style).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )
            jfa_second, _ = OutlineRenderer(jfa_style).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )
            poly_rendered, _ = OutlineRenderer(poly_style).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )

        self.assertEqual(jfa_diag.contours_rendered, 1)
        self.assertGreater(int(np.count_nonzero(jfa_first)), 0)
        self.assertFalse(np.array_equal(jfa_first, poly_rendered))
        self.assertTrue(np.array_equal(jfa_first, jfa_second))
        # jfa is accepted as a valid kernel
        self.assertEqual(OutlineStyle(outline_kernel="jfa").outline_kernel, "jfa")
        with self.assertRaisesRegex(ValueError, "outline_kernel"):
            OutlineStyle(outline_kernel="not-a-kernel")


if __name__ == "__main__":
    unittest.main()
