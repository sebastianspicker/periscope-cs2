from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import (
    ROLE_MARKERS,
    OutlineRenderer,
    OutlineStyle,
    RoleTreatment,
    TreatmentCatalog,
    load_treatment_catalog,
    treatment_catalog_from_mapping,
)


def _contour_area(contour: np.ndarray) -> float:
    points = contour.reshape((-1, 2)).astype(np.float64)
    shifted = np.roll(points, -1, axis=0)
    return abs(float(np.sum(points[:, 0] * shifted[:, 1] - shifted[:, 0] * points[:, 1]) / 2.0))


class RoleTreatmentCatalogTests(unittest.TestCase):
    def test_unknown_class_resolves_to_default(self) -> None:
        default = RoleTreatment(
            style=OutlineStyle(stroke_pattern="solid"),
            marker="none",
        )
        ally = RoleTreatment(
            style=OutlineStyle(
                inner_color="#00F5FF",
                outer_color="#050505",
                stroke_pattern="dashed",
            ),
            marker="triangle",
        )
        catalog = TreatmentCatalog(
            default=default,
            by_class_name={"ally": ally},
        )
        self.assertIs(catalog.resolve("ally"), ally)
        self.assertIs(catalog.resolve("ALLY"), ally)
        self.assertIs(catalog.resolve("enemy"), default)
        self.assertIs(catalog.resolve("player"), default)
        self.assertEqual(catalog.resolve_key("ally"), "ally")
        self.assertEqual(catalog.resolve_key("unknown"), "default")

    def test_color_only_ally_enemy_fails_validation(self) -> None:
        ally = RoleTreatment(
            style=OutlineStyle(
                inner_color="#00F5FF",
                outer_color="#050505",
                stroke_pattern="solid",
            ),
            marker="none",
        )
        enemy = RoleTreatment(
            style=OutlineStyle(
                inner_color="#F6FF00",
                outer_color="#101010",
                stroke_pattern="solid",
            ),
            marker="none",
        )
        with self.assertRaisesRegex(ValueError, "color-only"):
            TreatmentCatalog(
                default=RoleTreatment(style=OutlineStyle()),
                by_class_name={"ally": ally, "enemy": enemy},
            )

    def test_same_color_without_second_signifier_fails(self) -> None:
        style = OutlineStyle(stroke_pattern="solid")
        with self.assertRaisesRegex(ValueError, "color-only"):
            TreatmentCatalog(
                default=RoleTreatment(style=style),
                by_class_name={
                    "ally": RoleTreatment(style=style, marker="none"),
                    "enemy": RoleTreatment(style=style, marker="none"),
                },
            )

    def test_distinct_patterns_are_accepted(self) -> None:
        catalog = TreatmentCatalog(
            default=RoleTreatment(style=OutlineStyle()),
            by_class_name={
                "ally": RoleTreatment(
                    style=OutlineStyle(stroke_pattern="solid"),
                    marker="none",
                ),
                "enemy": RoleTreatment(
                    style=OutlineStyle(stroke_pattern="dashed"),
                    marker="none",
                ),
            },
        )
        self.assertEqual(catalog.resolve("ally").style.stroke_pattern, "solid")
        self.assertEqual(catalog.resolve("enemy").style.stroke_pattern, "dashed")

    def test_markers_allow_same_pattern_and_color(self) -> None:
        style = OutlineStyle(stroke_pattern="solid")
        catalog = TreatmentCatalog(
            default=RoleTreatment(style=style),
            by_class_name={
                "ally": RoleTreatment(style=style, marker="triangle"),
                "enemy": RoleTreatment(style=style, marker="square"),
            },
        )
        self.assertEqual(catalog.resolve("ally").marker, "triangle")
        self.assertEqual(catalog.resolve("enemy").marker, "square")

    def test_invalid_marker_and_scale_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "marker"):
            RoleTreatment(style=OutlineStyle(), marker="star")  # type: ignore[arg-type]
        with self.assertRaisesRegex(ValueError, "marker_scale"):
            RoleTreatment(style=OutlineStyle(), marker_scale=0)
        with self.assertRaisesRegex(ValueError, "marker_scale"):
            RoleTreatment(style=OutlineStyle(), marker_scale=-1.0)
        self.assertEqual(ROLE_MARKERS, frozenset({"none", "triangle", "square", "chevron"}))

    def test_casefold_duplicate_keys_fail(self) -> None:
        style = OutlineStyle(stroke_pattern="dashed")
        with self.assertRaisesRegex(ValueError, "duplicate"):
            TreatmentCatalog(
                default=RoleTreatment(style=OutlineStyle()),
                by_class_name={
                    "Ally": RoleTreatment(style=style, marker="triangle"),
                    "ally": RoleTreatment(
                        style=OutlineStyle(stroke_pattern="dotted"),
                        marker="square",
                    ),
                },
            )

    def test_json_load_and_example_catalog(self) -> None:
        example = Path(__file__).resolve().parents[1] / "docs" / "examples" / "role-catalog.v1.json"
        catalog = load_treatment_catalog(example)
        self.assertEqual(catalog.resolve("ally").marker, "triangle")
        self.assertEqual(catalog.resolve("enemy").style.stroke_pattern, "dashed")
        self.assertEqual(catalog.resolve("spectator").marker, "none")

        payload = {
            "schema_version": 1,
            "default": {
                "style": {"stroke_pattern": "solid"},
                "marker": "none",
            },
            "by_class_name": {
                "a": {"style": {"stroke_pattern": "dashed"}, "marker": "none"},
                "b": {"style": {"stroke_pattern": "dotted"}, "marker": "none"},
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "roles.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            loaded = load_treatment_catalog(path)
        self.assertEqual(loaded.resolve("a").style.stroke_pattern, "dashed")
        self.assertEqual(loaded.resolve("b").style.stroke_pattern, "dotted")

    def test_json_color_only_fails_closed(self) -> None:
        with self.assertRaisesRegex(ValueError, "color-only"):
            treatment_catalog_from_mapping(
                {
                    "schema_version": 1,
                    "default": {"style": {}, "marker": "none"},
                    "by_class_name": {
                        "ally": {
                            "style": {
                                "inner_color": "#00F5FF",
                                "outer_color": "#050505",
                                "stroke_pattern": "solid",
                            },
                            "marker": "none",
                        },
                        "enemy": {
                            "style": {
                                "inner_color": "#F6FF00",
                                "outer_color": "#101010",
                                "stroke_pattern": "solid",
                            },
                            "marker": "none",
                        },
                    },
                }
            )


class RoleRendererTests(unittest.TestCase):
    def _fake_cv2(self, *, track_fill: list | None = None) -> SimpleNamespace:
        fill_calls = track_fill if track_fill is not None else []

        def fill_poly(*_args: object, **_kwargs: object) -> None:
            fill_calls.append("fillPoly")

        return SimpleNamespace(
            LINE_AA=16,
            contourArea=_contour_area,
            fillPoly=fill_poly,
            addWeighted=lambda _overlay, _alpha, output, _beta, _gamma: output,
            polylines=lambda *_a, **_k: None,
            getStructuringElement=lambda *_a, **_k: None,
            dilate=lambda mask, *_a, **_k: mask,
            GaussianBlur=lambda mask, *_a, **_k: mask,
            MORPH_ELLIPSE=2,
        )

    def test_no_catalog_keeps_roles_applied_none(self) -> None:
        mask = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (20.0, 2.0), (11.0, 18.0)),
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        style = OutlineStyle(fill_opacity=0.0)
        with patch.dict(sys.modules, {"cv2": self._fake_cv2()}):
            _rendered, diagnostics = OutlineRenderer(style).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )
        self.assertIsNone(diagnostics.roles_applied)
        self.assertEqual(diagnostics.contours_rendered, 1)

    def test_unknown_class_uses_default_and_counts_roles(self) -> None:
        default = RoleTreatment(
            style=OutlineStyle(stroke_pattern="solid", fill_opacity=0.0),
            marker="none",
        )
        ally = RoleTreatment(
            style=OutlineStyle(
                inner_color="#00F5FF",
                outer_color="#050505",
                stroke_pattern="dashed",
                fill_opacity=0.0,
            ),
            marker="triangle",
        )
        catalog = TreatmentCatalog(
            default=default,
            by_class_name={"ally": ally},
        )
        unknown = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (20.0, 2.0), (11.0, 18.0)),
            confidence=0.9,
            class_id=1,
            class_name="spectator",
        )
        known = InstanceMask(
            frame_index=0,
            polygon=((22.0, 2.0), (40.0, 2.0), (31.0, 18.0)),
            confidence=0.9,
            class_id=0,
            class_name="Ally",
        )
        frame = np.zeros((48, 48, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": self._fake_cv2()}):
            _rendered, diagnostics = OutlineRenderer(
                OutlineStyle(), catalog=catalog
            ).render_with_diagnostics(frame, (unknown, known), frame_index=0)
        self.assertEqual(diagnostics.contours_rendered, 2)
        self.assertEqual(diagnostics.roles_applied, {"ally": 1, "default": 1})

    def test_two_classes_use_distinct_patterns(self) -> None:
        catalog = TreatmentCatalog(
            default=RoleTreatment(
                style=OutlineStyle(fill_opacity=0.0),
                marker="none",
            ),
            by_class_name={
                "ally": RoleTreatment(
                    style=OutlineStyle(
                        stroke_pattern="solid",
                        fill_opacity=0.0,
                    ),
                    marker="none",
                ),
                "enemy": RoleTreatment(
                    style=OutlineStyle(
                        stroke_pattern="dashed",
                        fill_opacity=0.0,
                        dash_period_px=8,
                    ),
                    marker="none",
                ),
            },
        )
        patterns_seen: list[str] = []

        def polylines(
            _image: object,
            contours: object,
            is_closed: object,
            *_rest: object,
            **_keywords: object,
        ) -> None:
            # solid closed True; dashed segments closed False
            if is_closed:
                patterns_seen.append("solid")
            else:
                patterns_seen.append("dashed")

        fake = self._fake_cv2()
        fake.polylines = polylines
        masks = (
            InstanceMask(
                frame_index=0,
                polygon=((2.0, 2.0), (30.0, 2.0), (30.0, 30.0), (2.0, 30.0)),
                confidence=0.9,
                class_id=0,
                class_name="ally",
            ),
            InstanceMask(
                frame_index=0,
                polygon=((40.0, 2.0), (70.0, 2.0), (70.0, 30.0), (40.0, 30.0)),
                confidence=0.9,
                class_id=1,
                class_name="enemy",
            ),
        )
        frame = np.zeros((48, 80, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": fake}):
            _rendered, diagnostics = OutlineRenderer(
                OutlineStyle(), catalog=catalog
            ).render_with_diagnostics(frame, masks, frame_index=0)
        self.assertEqual(diagnostics.roles_applied, {"ally": 1, "enemy": 1})
        self.assertIn("solid", patterns_seen)
        self.assertIn("dashed", patterns_seen)

    def test_marker_none_draws_no_markers(self) -> None:
        catalog = TreatmentCatalog(
            default=RoleTreatment(
                style=OutlineStyle(fill_opacity=0.0),
                marker="none",
            ),
            by_class_name={
                "ally": RoleTreatment(
                    style=OutlineStyle(
                        stroke_pattern="solid",
                        fill_opacity=0.0,
                    ),
                    marker="none",
                ),
                "enemy": RoleTreatment(
                    style=OutlineStyle(
                        stroke_pattern="dashed",
                        fill_opacity=0.0,
                    ),
                    marker="none",
                ),
            },
        )
        fill_calls: list[str] = []
        fake = self._fake_cv2(track_fill=fill_calls)
        mask = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (20.0, 2.0), (11.0, 18.0)),
            confidence=0.9,
            class_id=0,
            class_name="ally",
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": fake}):
            OutlineRenderer(OutlineStyle(), catalog=catalog).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )
        # fill_opacity=0 and marker=none no fillPoly (markers use fillPoly)
        self.assertEqual(fill_calls, [])

    def test_non_none_marker_emits_fill_poly(self) -> None:
        catalog = TreatmentCatalog(
            default=RoleTreatment(
                style=OutlineStyle(fill_opacity=0.0),
                marker="none",
            ),
            by_class_name={
                "ally": RoleTreatment(
                    style=OutlineStyle(fill_opacity=0.0),
                    marker="triangle",
                ),
            },
        )
        fill_calls: list[str] = []
        fake = self._fake_cv2(track_fill=fill_calls)
        mask = InstanceMask(
            frame_index=0,
            polygon=((2.0, 2.0), (20.0, 2.0), (11.0, 18.0)),
            confidence=0.9,
            class_id=0,
            class_name="ally",
        )
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        with patch.dict(sys.modules, {"cv2": fake}):
            OutlineRenderer(OutlineStyle(), catalog=catalog).render_with_diagnostics(
                frame, (mask,), frame_index=0
            )
        # dual fill (outer + inner marker) at minimum
        self.assertGreaterEqual(len(fill_calls), 2)


if __name__ == "__main__":
    unittest.main()
