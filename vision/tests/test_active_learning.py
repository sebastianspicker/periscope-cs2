"""Tests for multi-teacher geometry and active-learning ranking."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.training.active_learning import (
    UncertainFrame,
    rank_uncertain_frames,
    rank_uncertain_queue,
    score_uncertainty,
)
from cs2_vision_access.training.multi_teacher import (
    Box,
    box_from_polygon,
    box_iou,
    consensus_boxes,
    filter_temporally_unstable,
)
from tests.self_train_al_helpers import (
    OTHER,
    SQUARE,
    SQUARE_NEAR,
    TINY_BLIP,
    _mask,
)


class BoxIoUTests(unittest.TestCase):
    def test_identical_boxes_have_unit_iou(self) -> None:
        a = Box(10, 10, 40, 40)
        self.assertAlmostEqual(box_iou(a, a), 1.0, places=6)
        self.assertAlmostEqual(box_iou(a.xyxy, a.xyxy), 1.0, places=6)

    def test_disjoint_boxes_have_zero_iou(self) -> None:
        a = Box(0, 0, 10, 10)
        b = Box(20, 20, 30, 30)
        self.assertEqual(box_iou(a, b), 0.0)

    def test_partial_overlap_is_between_zero_and_one(self) -> None:
        a = Box(0, 0, 20, 20)
        b = Box(10, 10, 30, 30)
        iou = box_iou(a, b)
        self.assertGreater(iou, 0.0)
        self.assertLess(iou, 1.0)
        # Intersection 10x10=100; each area 400; union 700 → ~0.1429
        self.assertAlmostEqual(iou, 100.0 / 700.0, places=5)

    def test_box_from_polygon_aabb(self) -> None:
        box = box_from_polygon(SQUARE, class_id=2, confidence=0.7)
        self.assertEqual(box.xyxy, (10.0, 10.0, 40.0, 40.0))
        self.assertEqual(box.class_id, 2)
        self.assertEqual(box.confidence, 0.7)

    def test_invalid_box_rejected(self) -> None:
        with self.assertRaises(ValueError):
            Box(10, 10, 5, 20)
        with self.assertRaises(ValueError):
            Box(0, 0, 1, 1, confidence=1.5)


class ConsensusBoxesTests(unittest.TestCase):
    def test_single_teacher_returns_all_with_min_votes_1(self) -> None:
        dets = [[Box(0, 0, 10, 10, confidence=0.9), Box(50, 50, 60, 60, confidence=0.8)]]
        out = consensus_boxes(dets, iou_thresh=0.5)
        self.assertEqual(len(out), 2)

    def test_two_teachers_agree_on_one_box(self) -> None:
        t0 = [Box(0, 0, 20, 20, class_id=0, confidence=0.9)]
        t1 = [Box(1, 1, 21, 21, class_id=0, confidence=0.8)]
        out = consensus_boxes([t0, t1], iou_thresh=0.5)
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0].class_id, 0)
        self.assertAlmostEqual(out[0].confidence, 0.85, places=5)
        # Mean geometry
        self.assertAlmostEqual(out[0].x1, 0.5, places=5)

    def test_disagreement_drops_singleton_with_majority(self) -> None:
        t0 = [Box(0, 0, 20, 20, confidence=0.9)]
        t1 = [Box(100, 100, 120, 120, confidence=0.9)]  # no overlap
        t2 = [Box(0, 0, 19, 19, confidence=0.7)]  # agrees with t0
        # majority of 3 = 2
        out = consensus_boxes([t0, t1, t2], iou_thresh=0.5)
        self.assertEqual(len(out), 1)
        self.assertLess(out[0].x1, 50)

    def test_empty_teachers_returns_empty(self) -> None:
        self.assertEqual(consensus_boxes([]), [])
        self.assertEqual(consensus_boxes([[], []]), [])

    def test_min_votes_override(self) -> None:
        t0 = [Box(0, 0, 10, 10, confidence=0.9)]
        t1 = [Box(100, 100, 110, 110, confidence=0.9)]
        # Require both teachers — neither box has 2 votes
        out = consensus_boxes([t0, t1], iou_thresh=0.5, min_votes=2)
        self.assertEqual(out, [])


class TemporalFilterTests(unittest.TestCase):
    def test_single_frame_blip_dropped(self) -> None:
        frames = [
            [_mask(SQUARE)],
            [],  # dropout
            [_mask(OTHER)],  # different location — new track of length 1
        ]
        filtered = filter_temporally_unstable(frames, min_persist=2)
        self.assertEqual(filtered[0], [])
        self.assertEqual(filtered[1], [])
        self.assertEqual(filtered[2], [])

    def test_persisting_track_kept(self) -> None:
        frames = [
            [_mask(SQUARE, frame_index=0)],
            [_mask(SQUARE_NEAR, frame_index=1)],
            [_mask(SQUARE, frame_index=2)],
        ]
        filtered = filter_temporally_unstable(frames, min_persist=2, iou_thresh=0.3)
        self.assertEqual(len(filtered[0]), 1)
        self.assertEqual(len(filtered[1]), 1)
        self.assertEqual(len(filtered[2]), 1)

    def test_blip_among_stable_is_dropped(self) -> None:
        frames = [
            [_mask(SQUARE), _mask(TINY_BLIP, class_id=1, class_name="t")],
            [_mask(SQUARE_NEAR)],  # blip gone
            [_mask(SQUARE)],
        ]
        filtered = filter_temporally_unstable(frames, min_persist=2, iou_thresh=0.3)
        self.assertEqual(len(filtered[0]), 1)
        self.assertEqual(filtered[0][0].class_id, 0)
        self.assertEqual(len(filtered[1]), 1)
        self.assertEqual(len(filtered[2]), 1)

    def test_min_persist_1_is_identity(self) -> None:
        frames = [[_mask(SQUARE)], [_mask(OTHER)]]
        filtered = filter_temporally_unstable(frames, min_persist=1)
        self.assertEqual(len(filtered[0]), 1)
        self.assertEqual(len(filtered[1]), 1)

    def test_invalid_min_persist(self) -> None:
        with self.assertRaises(ValueError):
            filter_temporally_unstable([], min_persist=0)


class ScoreUncertaintyTests(unittest.TestCase):
    def test_low_student_conf_raises_score(self) -> None:
        teacher = [_mask(SQUARE, confidence=0.9)]
        student = [_mask(SQUARE_NEAR, confidence=0.2)]
        score, reason, max_conf, mean_iou = score_uncertainty(teacher, student)
        self.assertGreaterEqual(score, 0.8)
        self.assertEqual(max_conf, 0.2)
        self.assertIsNotNone(mean_iou)
        self.assertIn("low_conf", reason)

    def test_count_mismatch_is_max_uncertain(self) -> None:
        teacher = [_mask(SQUARE), _mask(OTHER, class_id=1, class_name="t")]
        student = [_mask(SQUARE_NEAR, confidence=0.95)]
        score, reason, _max_conf, _mean_iou = score_uncertainty(teacher, student)
        self.assertEqual(score, 1.0)
        self.assertIn("count_mismatch", reason)

    def test_empty_student_is_uncertain(self) -> None:
        teacher = [_mask(SQUARE)]
        score, reason, max_conf, mean_iou = score_uncertainty(teacher, [])
        self.assertEqual(score, 1.0)
        self.assertIsNone(max_conf)
        self.assertIsNone(mean_iou)
        self.assertIn("no_student_pred", reason)

    def test_agreeing_high_conf_is_low_uncertainty(self) -> None:
        teacher = [_mask(SQUARE, confidence=0.95)]
        student = [_mask(SQUARE_NEAR, confidence=0.95)]
        score, reason, max_conf, mean_iou = score_uncertainty(teacher, student)
        self.assertLess(score, 0.3)
        self.assertAlmostEqual(max_conf or 0.0, 0.95)
        self.assertIsNotNone(mean_iou)
        self.assertGreater(mean_iou or 0.0, 0.7)


class RankUncertainFramesTests(unittest.TestCase):
    def test_ranks_and_writes_jsonl(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            images.mkdir()
            (images / "hard.jpg").write_bytes(b"fake")
            (images / "easy.jpg").write_bytes(b"fake")
            queue = root / "queue.jsonl"

            def teacher_fn(path: Path) -> list[InstanceMask]:
                return [_mask(SQUARE, confidence=0.9)]

            def student_fn(path: Path) -> list[InstanceMask]:
                if path.stem == "hard":
                    return [_mask(SQUARE_NEAR, confidence=0.15)]
                return [_mask(SQUARE_NEAR, confidence=0.95)]

            ranked = rank_uncertain_frames(
                images,
                teacher_fn,
                student_fn,
                top_k=50,
                queue_path=queue,
            )
            self.assertEqual(len(ranked), 2)
            self.assertIsInstance(ranked[0], UncertainFrame)
            # Hard frame should rank first.
            self.assertEqual(ranked[0].stem, "hard")
            self.assertGreater(ranked[0].score, ranked[1].score)

            lines = queue.read_text(encoding="utf-8").strip().splitlines()
            self.assertEqual(len(lines), 2)
            first = json.loads(lines[0])
            self.assertEqual(first["stem"], "hard")
            self.assertIn("score", first)

    def test_top_k_limits_results(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            images = Path(tmp) / "images"
            images.mkdir()
            for name in ("a.jpg", "b.jpg", "c.jpg"):
                (images / name).write_bytes(b"x")

            ranked = rank_uncertain_frames(
                images,
                lambda _p: [_mask(SQUARE)],
                lambda _p: [],
                top_k=1,
            )
            self.assertEqual(len(ranked), 1)

    def test_cli_help(self) -> None:
        from cs2_vision_access.training.active_learning import main

        # argparse --help exits with SystemExit(0)
        with self.assertRaises(SystemExit) as ctx:
            main(["--help"])
        self.assertEqual(ctx.exception.code, 0)


class RankUncertainQueueTests(unittest.TestCase):
    def test_ranks_by_ascending_max_conf(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            queue = root / "uncertain_queue.jsonl"
            rows = [
                {"stem": "easy", "max_conf": 0.9, "path": "images/easy.jpg"},
                {"stem": "hard", "max_conf": 0.2, "path": "images/hard.jpg"},
                {"stem": "mid", "max_conf": 0.55, "path": "images/mid.jpg"},
            ]
            queue.write_text(
                "\n".join(json.dumps(r) for r in rows) + "\n",
                encoding="utf-8",
            )
            out_json = root / "progress" / "uncertain_review.json"
            out_jsonl = root / "progress" / "uncertain_review.jsonl"
            ranked = rank_uncertain_queue(queue, out_json, top_k=50, out_jsonl=out_jsonl)
            self.assertEqual([r["stem"] for r in ranked], ["hard", "mid", "easy"])
            self.assertAlmostEqual(float(ranked[0]["score"]), 0.8, places=5)
            self.assertTrue(out_json.is_file())
            self.assertTrue(out_jsonl.is_file())
            payload = json.loads(out_json.read_text(encoding="utf-8"))
            self.assertEqual(payload[0]["stem"], "hard")
            lines = out_jsonl.read_text(encoding="utf-8").strip().splitlines()
            self.assertEqual(len(lines), 3)

    def test_top_k_and_dedupe(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            queue = root / "q.jsonl"
            queue.write_text(
                "\n".join(
                    [
                        json.dumps({"stem": "a", "max_conf": 0.4}),
                        json.dumps({"stem": "a", "max_conf": 0.1}),  # keep lower conf
                        json.dumps({"stem": "b", "max_conf": 0.3}),
                        json.dumps({"stem": "c", "max_conf": 0.5}),
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            ranked = rank_uncertain_queue(queue, top_k=2)
            self.assertEqual(len(ranked), 2)
            self.assertEqual(ranked[0]["stem"], "a")
            self.assertAlmostEqual(float(ranked[0]["max_conf"]), 0.1, places=5)
            self.assertEqual(ranked[1]["stem"], "b")

    def test_missing_queue_returns_empty(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            ranked = rank_uncertain_queue(Path(tmp) / "missing.jsonl")
            self.assertEqual(ranked, [])


if __name__ == "__main__":
    unittest.main()
