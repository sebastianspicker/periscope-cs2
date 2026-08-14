"""Tests for self-train helpers and run_self_train_iteration."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.training.self_train import (
    SelfTrainError,
    SelfTrainReport,
    conf_band,
    instances_to_yolo_seg_lines,
    max_confidence,
    qc_yolo_seg_lines,
    run_self_train_iteration,
    write_label,
    write_label_if_absent,
)
from tests.self_train_al_helpers import OTHER, SQUARE, SQUARE_NEAR, _mask


class SelfTrainHelpersTests(unittest.TestCase):
    def test_instances_to_yolo_seg_lines_normalized(self) -> None:
        lines = instances_to_yolo_seg_lines(
            [_mask(SQUARE, class_id=1)],
            image_width=100,
            image_height=100,
        )
        self.assertEqual(len(lines), 1)
        tokens = lines[0].split()
        self.assertEqual(tokens[0], "1")
        # First point (10,10) → 0.1 0.1
        self.assertAlmostEqual(float(tokens[1]), 0.1, places=5)
        self.assertAlmostEqual(float(tokens[2]), 0.1, places=5)

    def test_max_confidence(self) -> None:
        self.assertIsNone(max_confidence([]))
        self.assertEqual(
            max_confidence([_mask(SQUARE, confidence=0.3), _mask(OTHER, confidence=0.8)]),
            0.8,
        )

    def test_write_label_if_absent_never_overwrites(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "frame.txt"
            self.assertTrue(write_label_if_absent(path, ["0 0.1 0.1 0.2 0.1 0.2 0.2"]))
            original = path.read_text(encoding="utf-8")
            self.assertFalse(write_label_if_absent(path, ["1 0.9 0.9 0.8 0.9 0.8 0.8"]))
            self.assertEqual(path.read_text(encoding="utf-8"), original)

    def test_write_label_overwrite_pseudo_revises_pseudo_but_not_gold(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pseudo = root / "pseudo.txt"
            gold = root / "gold.txt"
            empty = root / "empty.txt"

            self.assertTrue(
                write_label(
                    pseudo,
                    ["0 0.1 0.1 0.2 0.1 0.2 0.2 0.1 0.2"],
                    policy="if_absent",
                )
            )
            text = pseudo.read_text(encoding="utf-8")
            self.assertTrue(text.startswith("0 0.1"), text)
            self.assertTrue((root / "pseudo.txt.pseudo").is_file())

            revised = ["1 0.5 0.5 0.6 0.5 0.6 0.6 0.5 0.6"]
            self.assertTrue(write_label(pseudo, revised, policy="overwrite_pseudo"))
            revised_text = pseudo.read_text(encoding="utf-8")
            self.assertTrue(revised_text.startswith("1 0.5"), revised_text)
            self.assertNotIn("0 0.1", revised_text)
            self.assertTrue((root / "pseudo.txt.pseudo").is_file())

            gold.write_text("0 0.1 0.1 0.2 0.1 0.2 0.2 0.1 0.2\n", encoding="utf-8")
            gold_original = gold.read_text(encoding="utf-8")
            self.assertFalse(write_label(gold, revised, policy="overwrite_pseudo"))
            self.assertEqual(gold.read_text(encoding="utf-8"), gold_original)

            empty.write_text("", encoding="utf-8")
            self.assertTrue(write_label(empty, revised, policy="overwrite_pseudo"))
            self.assertTrue(empty.read_text(encoding="utf-8").startswith("1 0.5"))
            self.assertTrue((root / "empty.txt.pseudo").is_file())

            missing = root / "missing.txt"
            self.assertTrue(write_label(missing, revised, policy="overwrite_pseudo"))
            self.assertTrue(missing.is_file())
            self.assertTrue((root / "missing.txt.pseudo").is_file())

    def test_qc_drops_bad_class_and_tiny_polys(self) -> None:
        good = "0 0.1 0.1 0.5 0.1 0.5 0.5 0.1 0.5"
        bad_class = "99 0.1 0.1 0.5 0.1 0.5 0.5 0.1 0.5"
        # Tiny normalized bbox (width/height 1e-4 → area 1e-8)
        tiny = "0 0.5 0.5 0.5001 0.5 0.5001 0.5001 0.5 0.5001"
        # 5 tokens is YOLO-det; use <5 tokens for invalid line.
        too_few_points = "0 0.1 0.1 0.2"
        garbage = "not a label"
        det_ok = "1 0.5 0.5 0.2 0.2"  # area 0.04

        cleaned = qc_yolo_seg_lines(
            [good, bad_class, tiny, too_few_points, garbage, det_ok],
            allowed_class_ids={0, 1},
            min_points=3,
            min_area=1e-6,
        )
        self.assertEqual(len(cleaned), 2)
        self.assertTrue(cleaned[0].startswith("0 "))
        self.assertTrue(cleaned[1].startswith("1 "))
        # Det expanded to 4-corner poly (class + 8 coords)
        self.assertEqual(len(cleaned[1].split()), 1 + 8)

        no_allowed = qc_yolo_seg_lines([good], allowed_class_ids={2})
        self.assertEqual(no_allowed, [])

    def test_conf_band_helper(self) -> None:
        self.assertEqual(conf_band(0.1, conf_low=0.3, conf_high=0.8), "low")
        self.assertEqual(conf_band(0.5, conf_low=0.3, conf_high=0.8), "mid")
        self.assertEqual(conf_band(0.9, conf_low=0.3, conf_high=0.8), "high")
        self.assertEqual(conf_band(0.3, conf_low=0.3, conf_high=0.8), "mid")
        self.assertEqual(conf_band(0.8, conf_low=0.3, conf_high=0.8), "high")


class RunSelfTrainIterationTests(unittest.TestCase):
    def _fake_bgr(self, h: int = 64, w: int = 64) -> np.ndarray:
        return np.zeros((h, w, 3), dtype=np.uint8)

    def test_accepts_high_conf_and_writes_report(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "frame_a.jpg").write_bytes(b"not-a-real-image")
            (images / "frame_b.jpg").write_bytes(b"not-a-real-image")
            # frame_b already labeled — must not be overwritten
            (labels / "frame_b.txt").write_text("0 0.1 0.1 0.2 0.1 0.2 0.2\n", encoding="utf-8")
            prior = (labels / "frame_b.txt").read_text(encoding="utf-8")

            segmenter = MagicMock()
            segmenter.predict.return_value = (_mask(SQUARE, confidence=0.85, class_id=0),)

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model=root / "student.onnx",
                    student_manifest=root / "student.model.json",
                    conf_threshold=0.5,
                    segmenter=segmenter,
                )

            self.assertIsInstance(report, SelfTrainReport)
            self.assertEqual(report.images_scanned, 2)
            self.assertEqual(report.already_labeled, 1)
            self.assertEqual(report.unlabeled_scanned, 1)
            self.assertEqual(report.accepted, 1)
            self.assertEqual(report.rejected_low_conf, 0)
            self.assertTrue((labels / "frame_a.txt").is_file())
            body = (labels / "frame_a.txt").read_text(encoding="utf-8")
            self.assertTrue(body.startswith("0 "), body)
            self.assertTrue((labels / "frame_a.txt.pseudo").is_file())
            self.assertEqual((labels / "frame_b.txt").read_text(encoding="utf-8"), prior)

            report_path = Path(report.report_path or "")
            self.assertTrue(report_path.is_file())
            payload = json.loads(report_path.read_text(encoding="utf-8"))
            self.assertEqual(payload["accepted"], 1)
            self.assertEqual(payload["schema_version"], 1)
            self.assertEqual(payload["rejected_qc"], 0)
            self.assertEqual(payload["mid_band"], 0)

    def test_rejects_low_conf(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "weak.jpg").write_bytes(b"x")

            segmenter = MagicMock()
            segmenter.predict.return_value = (_mask(SQUARE, confidence=0.2),)

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=segmenter,
                )

            self.assertEqual(report.accepted, 0)
            self.assertEqual(report.rejected_low_conf, 1)
            self.assertFalse((labels / "weak.txt").exists())

    def test_rejects_empty_predictions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "empty.jpg").write_bytes(b"x")

            segmenter = MagicMock()
            segmenter.predict.return_value = ()

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=segmenter,
                )

            self.assertEqual(report.rejected_empty, 1)
            self.assertEqual(report.accepted, 0)

    def test_max_frames_caps_unlabeled(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            for name in ("a.jpg", "b.jpg", "c.jpg"):
                (images / name).write_bytes(b"x")

            segmenter = MagicMock()
            segmenter.predict.return_value = (_mask(SQUARE, confidence=0.9),)

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    max_frames=1,
                    segmenter=segmenter,
                )

            self.assertEqual(report.unlabeled_scanned, 1)
            self.assertEqual(report.accepted, 1)
            self.assertEqual(segmenter.predict.call_count, 1)

    def test_invalid_conf_threshold(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            with self.assertRaises(SelfTrainError):
                run_self_train_iteration(
                    root / "images",
                    root / "labels",
                    "m.onnx",
                    "m.json",
                    conf_threshold=1.5,
                    segmenter=MagicMock(),
                )

    def test_teacher_gate_rejects_disagreeing_student(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "disagree.jpg").write_bytes(b"x")

            student = MagicMock()
            # Student sees something at SQUARE
            student.predict.return_value = (_mask(SQUARE, confidence=0.9, class_id=0),)
            teacher = MagicMock()
            # Teacher sees a non-overlapping box of the same class
            teacher.predict.return_value = (_mask(OTHER, confidence=0.95, class_id=0),)

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(h=200, w=200),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=student,
                    teacher_segmenter=teacher,
                    teacher_min_iou=0.3,
                )

            self.assertEqual(report.accepted, 0)
            self.assertEqual(report.rejected_teacher, 1)
            self.assertFalse((labels / "disagree.txt").exists())

            # Agreeing teacher should pass
            teacher.predict.return_value = (_mask(SQUARE_NEAR, confidence=0.95, class_id=0),)
            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(h=200, w=200),
            ):
                report_ok = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=student,
                    teacher_segmenter=teacher,
                    teacher_min_iou=0.3,
                )
            self.assertEqual(report_ok.accepted, 1)
            self.assertEqual(report_ok.rejected_teacher, 0)
            self.assertTrue((labels / "disagree.txt").is_file())

    def test_teacher_empty_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "solo.jpg").write_bytes(b"x")

            student = MagicMock()
            student.predict.return_value = (_mask(SQUARE, confidence=0.9),)
            teacher = MagicMock()
            teacher.predict.return_value = ()

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=student,
                    teacher_segmenter=teacher,
                )
            self.assertEqual(report.rejected_teacher, 1)
            self.assertEqual(report.accepted, 0)

    def test_conf_mid_band_writes_queue_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "mid.jpg").write_bytes(b"x")
            (images / "high.jpg").write_bytes(b"x")
            queue = root / "uncertain.jsonl"

            def _predict(frame, frame_index=0):  # noqa: ANN001
                # Distinguish by which call — use a counter via mock side_effect
                return (_mask(SQUARE, confidence=0.55),)

            segmenter = MagicMock()
            # first image alphabetically is high.jpg, then mid.jpg
            segmenter.predict.side_effect = [
                (_mask(SQUARE, confidence=0.95),),  # high.jpg
                (_mask(SQUARE, confidence=0.55),),  # mid.jpg
            ]

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.3,
                    segmenter=segmenter,
                    conf_low=0.3,
                    conf_high=0.8,
                    uncertain_queue_path=queue,
                )

            self.assertEqual(report.accepted, 1)
            self.assertEqual(report.mid_band, 1)
            self.assertEqual(report.rejected_low_conf, 0)
            self.assertTrue((labels / "high.txt").is_file())
            self.assertFalse((labels / "mid.txt").exists())
            self.assertTrue(queue.is_file())
            rows = [
                json.loads(line) for line in queue.read_text(encoding="utf-8").strip().splitlines()
            ]
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["stem"], "mid")
            self.assertAlmostEqual(rows[0]["max_conf"], 0.55)
            self.assertIn("path", rows[0])

    def test_overwrite_pseudo_policy_in_iteration(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "rev.jpg").write_bytes(b"x")
            # Existing pseudo label (sidecar marker; pure YOLO body)
            (labels / "rev.txt").write_text(
                "0 0.1 0.1 0.2 0.1 0.2 0.2 0.1 0.2\n",
                encoding="utf-8",
            )
            (labels / "rev.txt.pseudo").write_text("pseudo\n", encoding="utf-8")

            segmenter = MagicMock()
            segmenter.predict.return_value = (_mask(SQUARE, confidence=0.9, class_id=1),)

            with patch(
                "cs2_vision_access.training.self_train.iteration.cv2.imread",
                return_value=self._fake_bgr(),
            ):
                report = run_self_train_iteration(
                    images,
                    labels,
                    student_model="ignored.onnx",
                    student_manifest="ignored.json",
                    conf_threshold=0.5,
                    segmenter=segmenter,
                    write_policy="overwrite_pseudo",
                )

            self.assertEqual(report.accepted, 1)
            self.assertEqual(report.overwritten, 1)
            content = (labels / "rev.txt").read_text(encoding="utf-8")
            self.assertTrue(content.startswith("1 "), content)
            self.assertTrue((labels / "rev.txt.pseudo").is_file())


if __name__ == "__main__":
    unittest.main()
