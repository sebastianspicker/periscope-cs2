from __future__ import annotations

import json
import math
import tempfile
import unittest
from pathlib import Path

import numpy as np

from cs2_vision_access.evaluation import (
    SCHEMA_VERSION,
    EvaluationError,
    FrameEvaluation,
    PolygonInstance,
    TemporalFrame,
    boundary_f1,
    clutter_fraction,
    evaluate_comfort,
    evaluate_negatives,
    evaluate_negatives_from_files,
    evaluate_predictions,
    evaluate_temporal,
    evaluate_temporal_from_files,
    mask_iou,
    match_instances,
    negatives_counts_from_predictions,
    polygon_area_px,
    polygon_centroid,
    raster_area_px,
    recall_by_area_bin,
    temporal_frames_from_payload,
)

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "eval_masks"


class EvaluationMetricTests(unittest.TestCase):
    def test_identical_axis_aligned_boxes_have_unit_iou(self) -> None:
        box = ((10.0, 10.0), (40.0, 10.0), (40.0, 40.0), (10.0, 40.0))
        iou = mask_iou(box, box, height=64, width=64)
        self.assertAlmostEqual(iou, 1.0, places=5)

    def test_disjoint_boxes_have_zero_iou(self) -> None:
        left = ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0))
        right = ((20.0, 20.0), (30.0, 20.0), (30.0, 30.0), (20.0, 30.0))
        self.assertEqual(mask_iou(left, right, height=64, width=64), 0.0)

    def test_identical_boxes_have_high_boundary_f1(self) -> None:
        box = ((8.0, 8.0), (40.0, 8.0), (40.0, 40.0), (8.0, 40.0))
        score = boundary_f1(box, box, height=64, width=64, dilation_px=2)
        self.assertGreaterEqual(score, 0.99)

    def test_empty_empty_boundary_f1_is_zero_like_iou(self) -> None:
        line = ((1.0, 1.0), (5.0, 1.0), (9.0, 1.0))
        self.assertEqual(mask_iou(line, line, height=16, width=16), 0.0)
        self.assertEqual(
            boundary_f1(line, line, height=16, width=16, dilation_px=2),
            0.0,
        )

    def test_zero_area_polygon_instance_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "area must be positive"):
            PolygonInstance(0, ((0.0, 0.0), (10.0, 0.0), (20.0, 0.0)))

    def test_subpixel_positive_shoelace_zero_raster_is_rejected(self) -> None:
        subpixel = ((5.1, 5.1), (5.2, 5.1), (5.15, 5.2))
        self.assertGreater(polygon_area_px(subpixel), 0.0)
        self.assertEqual(raster_area_px(subpixel, height=64, width=64), 0.0)
        with self.assertRaisesRegex(ValueError, "raster area must be positive"):
            FrameEvaluation(
                image_id="tiny",
                width=64,
                height=64,
                ground_truth=(PolygonInstance(0, subpixel),),
                predictions=(),
            )

    def test_empty_empty_never_matches_even_at_zero_threshold(self) -> None:
        left = PolygonInstance(0, ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0)))
        right = PolygonInstance(0, ((20.0, 20.0), (30.0, 20.0), (30.0, 30.0), (20.0, 30.0)))
        matches = match_instances(
            (left,),
            (right,),
            height=64,
            width=64,
            iou_threshold=0.0,
        )
        self.assertEqual(matches, ())

    def test_greedy_matching_is_one_to_one(self) -> None:
        gt = (
            PolygonInstance(0, ((0.0, 0.0), (20.0, 0.0), (20.0, 20.0), (0.0, 20.0))),
            PolygonInstance(0, ((30.0, 30.0), (50.0, 30.0), (50.0, 50.0), (30.0, 50.0))),
        )
        predictions = (
            PolygonInstance(0, ((1.0, 1.0), (21.0, 1.0), (21.0, 21.0), (1.0, 21.0))),
            PolygonInstance(0, ((31.0, 31.0), (51.0, 31.0), (51.0, 51.0), (31.0, 51.0))),
            PolygonInstance(0, ((0.0, 40.0), (10.0, 40.0), (10.0, 50.0), (0.0, 50.0))),
        )
        matches = match_instances(gt, predictions, height=64, width=64, iou_threshold=0.3)
        self.assertEqual(len(matches), 2)
        self.assertEqual({gt_index for gt_index, _, _ in matches}, {0, 1})
        self.assertEqual(len({pred_index for _, pred_index, _ in matches}), 2)

    def test_class_mismatch_is_not_matched(self) -> None:
        gt = (PolygonInstance(0, ((0.0, 0.0), (20.0, 0.0), (20.0, 20.0), (0.0, 20.0))),)
        predictions = (PolygonInstance(1, ((0.0, 0.0), (20.0, 0.0), (20.0, 20.0), (0.0, 20.0))),)
        matches = match_instances(gt, predictions, height=64, width=64)
        self.assertEqual(matches, ())

    def test_recall_by_area_bin_uses_raster_area(self) -> None:
        tiny = PolygonInstance(0, ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0)))
        large = PolygonInstance(0, ((0.0, 0.0), (200.0, 0.0), (200.0, 200.0), (0.0, 200.0)))
        tiny_area = raster_area_px(tiny.polygon, height=256, width=256)
        large_area = raster_area_px(large.polygon, height=256, width=256)
        self.assertLess(tiny_area, 1_024.0)
        self.assertGreaterEqual(large_area, 16_384.0)
        bins = recall_by_area_bin((tiny_area, large_area), matched_gt_indices=(0,))
        by_name = {entry.name: entry for entry in bins}
        self.assertEqual(by_name["tiny"].ground_truth_count, 1)
        self.assertEqual(by_name["tiny"].true_positives, 1)
        self.assertEqual(by_name["tiny"].recall, 1.0)
        self.assertEqual(by_name["large"].ground_truth_count, 1)
        self.assertEqual(by_name["large"].true_positives, 0)
        self.assertEqual(by_name["large"].recall, 0.0)

    def test_partial_boundary_f1_drops_without_dilation(self) -> None:
        gt = ((8.0, 8.0), (32.0, 8.0), (32.0, 32.0), (8.0, 32.0))
        pred = ((10.0, 10.0), (34.0, 10.0), (34.0, 34.0), (10.0, 34.0))
        dilated = boundary_f1(gt, pred, height=64, width=64, dilation_px=2)
        strict = boundary_f1(gt, pred, height=64, width=64, dilation_px=0)
        self.assertGreaterEqual(dilated, 0.99)
        self.assertLess(strict, 1.0)
        self.assertGreater(strict, 0.0)

    def test_evaluate_predictions_identical_geometry_is_perfect(self) -> None:
        box = ((16.0, 16.0), (48.0, 16.0), (48.0, 48.0), (16.0, 48.0))
        frame = FrameEvaluation(
            image_id="identical",
            width=64,
            height=64,
            ground_truth=(PolygonInstance(0, box),),
            predictions=(PolygonInstance(0, box, confidence=0.95),),
        )
        result = evaluate_predictions((frame,))
        self.assertEqual(result.schema_version, SCHEMA_VERSION)
        self.assertEqual(result.true_positives, 1)
        self.assertEqual(result.false_positives, 0)
        self.assertEqual(result.false_negatives, 0)
        self.assertAlmostEqual(result.precision, 1.0)
        self.assertAlmostEqual(result.recall, 1.0)
        self.assertAlmostEqual(result.f1, 1.0)
        self.assertAlmostEqual(result.mean_matched_iou, 1.0, places=5)
        self.assertGreaterEqual(result.mean_boundary_f1, 0.99)
        payload = result.as_dict()
        for key in (
            "precision",
            "recall",
            "f1",
            "mean_matched_iou",
            "mean_boundary_f1",
        ):
            value = payload[key]
            self.assertIsInstance(value, float)
            self.assertTrue(math.isfinite(value))

    def test_empty_frames_yield_finite_zero_rates(self) -> None:
        frame = FrameEvaluation(
            image_id="empty",
            width=32,
            height=32,
            ground_truth=(),
            predictions=(),
        )
        result = evaluate_predictions((frame,))
        self.assertEqual(result.precision, 0.0)
        self.assertEqual(result.recall, 0.0)
        self.assertEqual(result.f1, 0.0)
        self.assertEqual(result.mean_matched_iou, 0.0)
        self.assertEqual(result.mean_boundary_f1, 0.0)


class NegativesMetricTests(unittest.TestCase):
    def test_empty_counts_yield_finite_zero_rates(self) -> None:
        result = evaluate_negatives((), fps=30.0)
        self.assertEqual(result.schema_version, SCHEMA_VERSION)
        self.assertEqual(result.frame_count, 0)
        self.assertEqual(result.false_positive_count, 0)
        self.assertEqual(result.duration_seconds, 0.0)
        self.assertEqual(result.false_positives_per_minute, 0.0)
        self.assertTrue(math.isfinite(result.false_positives_per_minute))

    def test_zero_fp_frames_yield_zero_rate(self) -> None:
        result = evaluate_negatives((0, 0, 0), fps=30.0)
        self.assertEqual(result.frame_count, 3)
        self.assertEqual(result.false_positive_count, 0)
        self.assertAlmostEqual(result.duration_seconds, 0.1)
        self.assertEqual(result.false_positives_per_minute, 0.0)

    def test_fp_per_minute_formula(self) -> None:
        result = evaluate_negatives((1, 0, 2, 0, 3, 0) + (0,) * 54, fps=30.0)
        self.assertEqual(result.frame_count, 60)
        self.assertEqual(result.false_positive_count, 6)
        self.assertAlmostEqual(result.duration_seconds, 2.0)
        self.assertAlmostEqual(result.false_positives_per_minute, 180.0)

    def test_expected_zero_selection_from_predictions(self) -> None:
        payload = {
            "schema_version": 1,
            "images": {
                "clean": {
                    "width": 32,
                    "height": 32,
                    "expected_zero": True,
                    "predictions": [],
                },
                "noisy": {
                    "width": 32,
                    "height": 32,
                    "expected_zero": True,
                    "predictions": [
                        {
                            "class_id": 0,
                            "confidence": 0.5,
                            "polygon": [
                                [2.0, 2.0],
                                [12.0, 2.0],
                                [12.0, 12.0],
                                [2.0, 12.0],
                            ],
                        }
                    ],
                },
                "ignored_positive": {
                    "width": 32,
                    "height": 32,
                    "predictions": [
                        {
                            "class_id": 0,
                            "confidence": 0.9,
                            "polygon": [
                                [2.0, 2.0],
                                [12.0, 2.0],
                                [12.0, 12.0],
                                [2.0, 12.0],
                            ],
                        }
                    ],
                },
            },
        }
        counts, selection = negatives_counts_from_predictions(payload)
        self.assertEqual(selection, "expected_zero")
        self.assertEqual(counts, (0, 1))
        result = evaluate_negatives(counts, fps=30.0, selection=selection)
        self.assertEqual(result.false_positive_count, 1)

    def test_empty_labels_selection_with_dataset(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            labels = root / "labels" / "test"
            labels.mkdir(parents=True)
            (labels / "empty_a.txt").write_text("", encoding="utf-8")
            (labels / "empty_b.txt").write_text("\n\n", encoding="utf-8")
            (labels / "has_player.txt").write_text(
                "0 0.25 0.25 0.75 0.25 0.75 0.75 0.25 0.75\n",
                encoding="utf-8",
            )
            predictions_path = root / "predictions.v1.json"
            predictions_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "default_width": 64,
                        "default_height": 64,
                        "images": {
                            "empty_a": {
                                "width": 64,
                                "height": 64,
                                "predictions": [
                                    {
                                        "class_id": 0,
                                        "confidence": 0.4,
                                        "polygon": [
                                            [4.0, 4.0],
                                            [20.0, 4.0],
                                            [20.0, 20.0],
                                            [4.0, 20.0],
                                        ],
                                    }
                                ],
                            },
                            "empty_b": {
                                "width": 64,
                                "height": 64,
                                "predictions": [],
                            },
                        },
                    },
                    indent=2,
                    sort_keys=True,
                )
                + "\n",
                encoding="utf-8",
            )
            result = evaluate_negatives_from_files(
                predictions_path=predictions_path,
                fps=30.0,
                dataset_root=root,
                split="test",
            )
            self.assertEqual(result.selection, "empty_labels")
            self.assertEqual(result.frame_count, 2)
            self.assertEqual(result.false_positive_count, 1)
            self.assertTrue(math.isfinite(result.false_positives_per_minute))
            self.assertGreater(result.false_positives_per_minute, 0.0)

    def test_reject_missing_selection(self) -> None:
        payload = {
            "schema_version": 1,
            "images": {
                "frame": {"width": 16, "height": 16, "predictions": []},
            },
        }
        with self.assertRaisesRegex(EvaluationError, "no expected-empty"):
            negatives_counts_from_predictions(payload)

    def test_reject_non_positive_fps(self) -> None:
        with self.assertRaisesRegex(EvaluationError, "fps"):
            evaluate_negatives((0,), fps=0.0)


class TemporalMetricTests(unittest.TestCase):
    def _box(
        self, x0: float, y0: float, x1: float, y1: float, class_id: int = 0
    ) -> PolygonInstance:
        return PolygonInstance(
            class_id,
            ((x0, y0), (x1, y0), (x1, y1), (x0, y1)),
        )

    def test_empty_sequence_yields_finite_zeros(self) -> None:
        result = evaluate_temporal(())
        self.assertEqual(result.schema_version, SCHEMA_VERSION)
        self.assertEqual(result.frame_count, 0)
        self.assertEqual(result.matched_pair_count, 0)
        self.assertEqual(result.mean_centroid_displacement_px, 0.0)
        self.assertEqual(result.mean_iou_drop, 0.0)
        self.assertEqual(result.presence_flicker_rate, 0.0)
        for value in (
            result.mean_centroid_displacement_px,
            result.mean_iou_drop,
            result.presence_flicker_rate,
        ):
            self.assertTrue(math.isfinite(value))

    def test_stable_identical_instances_have_zero_instability(self) -> None:
        box = self._box(10, 10, 30, 30)
        frames = (
            TemporalFrame(0, 64, 64, (box,)),
            TemporalFrame(1, 64, 64, (box,)),
            TemporalFrame(2, 64, 64, (box,)),
        )
        result = evaluate_temporal(frames, iou_threshold=0.5)
        self.assertEqual(result.matched_pair_count, 2)
        self.assertAlmostEqual(result.mean_centroid_displacement_px, 0.0, places=5)
        self.assertAlmostEqual(result.mean_iou_drop, 0.0, places=5)
        self.assertEqual(result.presence_flicker_rate, 0.0)

    def test_translated_instance_reports_centroid_displacement(self) -> None:
        frames = (
            TemporalFrame(0, 64, 64, (self._box(10, 10, 30, 30),)),
            TemporalFrame(1, 64, 64, (self._box(14, 10, 34, 30),)),
        )
        result = evaluate_temporal(frames, iou_threshold=0.3)
        self.assertEqual(result.matched_pair_count, 1)
        self.assertAlmostEqual(result.mean_centroid_displacement_px, 4.0, places=4)
        self.assertGreater(result.mean_iou_drop, 0.0)
        self.assertLess(result.mean_iou_drop, 1.0)
        self.assertEqual(result.presence_flicker_rate, 0.0)

    def test_presence_flicker_when_detection_drops(self) -> None:
        frames = (
            TemporalFrame(0, 64, 64, (self._box(10, 10, 30, 30),)),
            TemporalFrame(1, 64, 64, ()),
            TemporalFrame(2, 64, 64, (self._box(10, 10, 30, 30),)),
        )
        result = evaluate_temporal(frames, iou_threshold=0.5)
        self.assertEqual(result.matched_pair_count, 0)
        self.assertEqual(result.presence_flicker_rate, 1.0)

    def test_polygon_centroid_of_axis_aligned_box(self) -> None:
        cx, cy = polygon_centroid(((0.0, 0.0), (10.0, 0.0), (10.0, 20.0), (0.0, 20.0)))
        self.assertAlmostEqual(cx, 5.0, places=5)
        self.assertAlmostEqual(cy, 10.0, places=5)

    def test_sequence_payload_and_file_roundtrip(self) -> None:
        payload = {
            "schema_version": 1,
            "frames": [
                {
                    "frame_index": 0,
                    "width": 32,
                    "height": 32,
                    "predictions": [
                        {
                            "class_id": 0,
                            "confidence": 0.9,
                            "polygon": [
                                [4.0, 4.0],
                                [16.0, 4.0],
                                [16.0, 16.0],
                                [4.0, 16.0],
                            ],
                        }
                    ],
                },
                {
                    "frame_index": 1,
                    "width": 32,
                    "height": 32,
                    "predictions": [
                        {
                            "class_id": 0,
                            "confidence": 0.9,
                            "polygon": [
                                [5.0, 4.0],
                                [17.0, 4.0],
                                [17.0, 16.0],
                                [5.0, 16.0],
                            ],
                        }
                    ],
                },
            ],
        }
        frames = temporal_frames_from_payload(payload)
        self.assertEqual(len(frames), 2)
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "sequence.v1.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            result = evaluate_temporal_from_files(sequence_path=path)
            self.assertEqual(result.schema_version, 1)
            self.assertEqual(result.matched_pair_count, 1)
            self.assertAlmostEqual(result.mean_centroid_displacement_px, 1.0, places=4)


class ComfortMetricTests(unittest.TestCase):
    def test_empty_predictions_finite_zeros(self) -> None:
        frame = np.full((32, 32, 3), 128, dtype=np.uint8)
        result = evaluate_comfort(frame, ())
        self.assertEqual(result.schema_version, SCHEMA_VERSION)
        self.assertEqual(result.prediction_count, 0)
        self.assertEqual(result.clutter_fraction, 0.0)
        self.assertEqual(result.local_stroke_contrast_ge_3_fraction, 0.0)
        self.assertEqual(result.contrast_sample_count, 0)
        self.assertTrue(math.isfinite(result.clutter_fraction))
        self.assertTrue(math.isfinite(result.local_stroke_contrast_ge_3_fraction))

    def test_clutter_fraction_of_full_frame_box(self) -> None:
        poly = ((0.0, 0.0), (16.0, 0.0), (16.0, 16.0), (0.0, 16.0))
        fraction = clutter_fraction((poly,), height=16, width=16)
        self.assertGreater(fraction, 0.5)
        self.assertLessEqual(fraction, 1.0)

    def test_high_contrast_outer_on_light_background(self) -> None:
        frame = np.full((64, 64, 3), 255, dtype=np.uint8)
        instance = PolygonInstance(
            0,
            ((16.0, 16.0), (48.0, 16.0), (48.0, 48.0), (16.0, 48.0)),
        )
        result = evaluate_comfort(
            frame,
            (instance,),
            outer_color="#101010",
            inner_color="#F6FF00",
        )
        self.assertGreater(result.clutter_fraction, 0.0)
        self.assertGreater(result.contrast_sample_count, 0)
        self.assertGreaterEqual(result.local_stroke_contrast_ge_3_fraction, 0.9)
        payload = result.as_dict()
        self.assertEqual(payload["schema_version"], 1)
        self.assertTrue(math.isfinite(payload["clutter_fraction"]))
        self.assertTrue(math.isfinite(payload["local_stroke_contrast_ge_3_fraction"]))

    def test_low_contrast_outer_on_dark_background(self) -> None:
        frame = np.full((64, 64, 3), 16, dtype=np.uint8)
        instance = PolygonInstance(
            0,
            ((16.0, 16.0), (48.0, 16.0), (48.0, 48.0), (16.0, 48.0)),
        )
        result = evaluate_comfort(
            frame,
            (instance,),
            outer_color="#101010",
            inner_color="#F6FF00",
        )
        self.assertGreater(result.contrast_sample_count, 0)
        self.assertLess(result.local_stroke_contrast_ge_3_fraction, 0.5)
