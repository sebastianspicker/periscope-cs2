from __future__ import annotations

import json
import math
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.evaluation import (
    EvaluationError,
    evaluate_masks_from_files,
    frames_from_predictions_and_dataset,
    load_yolo_polygons,
    mask_iou,
)

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "eval_masks"


class YoloLoadAndFixtureTests(unittest.TestCase):
    def test_load_yolo_polygons_converts_normalized_coords(self) -> None:
        label = FIXTURES / "labels" / "test" / "identical.txt"
        instances = load_yolo_polygons(label, image_width=64, image_height=64)
        self.assertEqual(len(instances), 1)
        self.assertEqual(instances[0].class_id, 0)
        self.assertEqual(instances[0].polygon[0], (16.0, 16.0))
        self.assertEqual(instances[0].polygon[2], (48.0, 48.0))

    def test_fixture_eval_masks_are_finite_and_discriminating(self) -> None:
        result = evaluate_masks_from_files(
            predictions_path=FIXTURES / "predictions.v1.json",
            dataset_root=FIXTURES,
            split="test",
        )
        self.assertEqual(result.schema_version, 1)
        self.assertEqual(result.image_count, 3)
        self.assertEqual(result.ground_truth_count, 2)
        self.assertEqual(result.prediction_count, 2)
        self.assertEqual(result.true_positives, 2)
        self.assertEqual(result.false_positives, 0)
        self.assertEqual(result.false_negatives, 0)
        self.assertAlmostEqual(result.precision, 1.0)
        self.assertAlmostEqual(result.recall, 1.0)
        self.assertAlmostEqual(result.mean_matched_iou, 0.86, places=2)
        self.assertLess(result.mean_matched_iou, 1.0)

        partial_gt = ((8.0, 8.0), (32.0, 8.0), (32.0, 32.0), (8.0, 32.0))
        partial_pred = ((10.0, 10.0), (34.0, 10.0), (34.0, 34.0), (10.0, 34.0))
        partial_iou = mask_iou(partial_gt, partial_pred, height=64, width=64)
        self.assertGreater(partial_iou, 0.5)
        self.assertLess(partial_iou, 1.0)

        for value in (
            result.precision,
            result.recall,
            result.f1,
            result.mean_matched_iou,
            result.mean_boundary_f1,
        ):
            self.assertTrue(math.isfinite(value))

    def test_label_stem_absent_from_predictions_counts_as_false_negative(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            labels = root / "labels" / "test"
            labels.mkdir(parents=True)
            (labels / "covered.txt").write_text(
                "0 0.25 0.25 0.75 0.25 0.75 0.75 0.25 0.75\n",
                encoding="utf-8",
            )
            (labels / "omitted.txt").write_text(
                "0 0.10 0.10 0.40 0.10 0.40 0.40 0.10 0.40\n",
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
                            "covered": {
                                "width": 64,
                                "height": 64,
                                "predictions": [
                                    {
                                        "class_id": 0,
                                        "confidence": 0.9,
                                        "polygon": [
                                            [16.0, 16.0],
                                            [48.0, 16.0],
                                            [48.0, 48.0],
                                            [16.0, 48.0],
                                        ],
                                    }
                                ],
                            }
                        },
                    },
                    indent=2,
                    sort_keys=True,
                )
                + "\n",
                encoding="utf-8",
            )
            result = evaluate_masks_from_files(
                predictions_path=predictions_path,
                dataset_root=root,
                split="test",
            )
            self.assertEqual(result.image_count, 2)
            self.assertEqual(result.ground_truth_count, 2)
            self.assertEqual(result.prediction_count, 1)
            self.assertEqual(result.true_positives, 1)
            self.assertEqual(result.false_positives, 0)
            self.assertGreaterEqual(result.false_negatives, 1)
            self.assertEqual(result.false_negatives, 1)
            self.assertAlmostEqual(result.recall, 0.5)

    def test_path_escape_image_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "labels" / "test").mkdir(parents=True)
            (root / "secret.txt").write_text(
                "0 0.25 0.25 0.75 0.25 0.75 0.75 0.25 0.75\n",
                encoding="utf-8",
            )
            payload = {
                "schema_version": 1,
                "images": {
                    "../../secret": {
                        "width": 64,
                        "height": 64,
                        "predictions": [],
                    }
                },
            }
            with self.assertRaisesRegex(EvaluationError, "path separators|path segment"):
                frames_from_predictions_and_dataset(payload, root, split="test")

    def test_path_escape_split_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "labels" / "test").mkdir(parents=True)
            payload = {
                "schema_version": 1,
                "images": {
                    "frame": {
                        "width": 64,
                        "height": 64,
                        "predictions": [],
                    }
                },
            }
            with self.assertRaisesRegex(EvaluationError, "path separators|path segment"):
                frames_from_predictions_and_dataset(payload, root, split="../test")

    def test_malformed_label_raises(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "bad.txt"
            path.write_text("0 0.1 0.1\n", encoding="utf-8")
            with self.assertRaises(EvaluationError):
                load_yolo_polygons(path, image_width=64, image_height=64)
