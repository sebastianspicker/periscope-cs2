"""Unit tests for shared multi-iter self-train helpers."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.training.active_learning import rank_uncertain_queue_soft
from cs2_vision_access.training.dataset_zip import resolve_self_train_dirs
from cs2_vision_access.training.self_train_schedule import (
    growth_plateau_update,
    resolve_conf_bands,
    scheduled_self_train_conf,
)


class ScheduledConfTests(unittest.TestCase):
    def test_disabled_returns_base(self) -> None:
        self.assertAlmostEqual(
            scheduled_self_train_conf(0.5, 5, enabled=False),
            0.5,
        )

    def test_iteration_one_is_base(self) -> None:
        self.assertAlmostEqual(
            scheduled_self_train_conf(0.5, 1, enabled=True),
            0.5,
        )

    def test_growth_formula(self) -> None:
        # min(0.85, 0.5 * (1 + 0.05 * (3 - 1))) = 0.5 * 1.1 = 0.55
        self.assertAlmostEqual(
            scheduled_self_train_conf(0.5, 3, enabled=True),
            0.55,
        )

    def test_cap_applied(self) -> None:
        # base 0.8, it=5 → 0.8 * (1 + 0.05*4) = 0.96 → capped 0.85
        self.assertAlmostEqual(
            scheduled_self_train_conf(0.8, 5, enabled=True),
            0.85,
        )

    def test_sub_one_iteration_clamped(self) -> None:
        self.assertAlmostEqual(
            scheduled_self_train_conf(0.4, 0, enabled=True),
            0.4,
        )


class ResolveConfBandsTests(unittest.TestCase):
    def test_defaults_without_schedule(self) -> None:
        conf, low, high = resolve_conf_bands(0.5, iteration=3, conf_schedule=False)
        self.assertAlmostEqual(conf, 0.5)
        self.assertAlmostEqual(high, 0.5)
        self.assertAlmostEqual(low, 0.35)

    def test_defaults_with_schedule(self) -> None:
        conf, low, high = resolve_conf_bands(0.5, iteration=3, conf_schedule=True)
        self.assertAlmostEqual(conf, 0.55)
        self.assertAlmostEqual(high, 0.55)
        self.assertAlmostEqual(low, 0.55 * 0.7)

    def test_explicit_bases_are_scheduled(self) -> None:
        conf, low, high = resolve_conf_bands(
            0.5,
            conf_low=0.3,
            conf_high=0.6,
            iteration=3,
            conf_schedule=True,
        )
        self.assertAlmostEqual(conf, 0.55)
        # 0.6 * 1.1 = 0.66
        self.assertAlmostEqual(high, 0.66)
        # 0.3 * 1.1 = 0.33
        self.assertAlmostEqual(low, 0.33)

    def test_low_clamped_to_high(self) -> None:
        conf, low, high = resolve_conf_bands(
            0.5,
            conf_low=0.9,
            conf_high=0.4,
            iteration=1,
            conf_schedule=False,
        )
        self.assertAlmostEqual(high, 0.4)
        self.assertAlmostEqual(low, 0.4)
        self.assertAlmostEqual(conf, 0.5)


class GrowthPlateauTests(unittest.TestCase):
    def test_accept_resets_plateau(self) -> None:
        plateau, reason = growth_plateau_update(
            3,
            accepted=2,
            stop_on_no_growth=True,
            max_plateau_iters=2,
        )
        self.assertEqual(plateau, 0)
        self.assertIsNone(reason)

    def test_zero_increments_until_stop(self) -> None:
        p, r = growth_plateau_update(0, accepted=0, stop_on_no_growth=True, max_plateau_iters=2)
        self.assertEqual(p, 1)
        self.assertIsNone(r)
        p, r = growth_plateau_update(p, accepted=0, stop_on_no_growth=True, max_plateau_iters=2)
        self.assertEqual(p, 2)
        self.assertEqual(r, "plateau")

    def test_max_one_is_no_growth(self) -> None:
        p, r = growth_plateau_update(0, accepted=0, stop_on_no_growth=True, max_plateau_iters=1)
        self.assertEqual(p, 1)
        self.assertEqual(r, "no_growth")

    def test_stop_disabled_leaves_plateau(self) -> None:
        p, r = growth_plateau_update(4, accepted=0, stop_on_no_growth=False, max_plateau_iters=2)
        self.assertEqual(p, 4)
        self.assertIsNone(r)


class ResolveSelfTrainDirsTests(unittest.TestCase):
    def test_prefers_train_split(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images" / "train").mkdir(parents=True)
            (root / "labels" / "train").mkdir(parents=True)
            (root / "images" / "val").mkdir(parents=True)
            (root / "labels" / "val").mkdir(parents=True)
            resolved = resolve_self_train_dirs(root)
            assert resolved is not None
            images, labels = resolved
            self.assertEqual(images, root / "images" / "train")
            self.assertEqual(labels, root / "labels" / "train")

    def test_flat_when_no_train_tree(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            resolved = resolve_self_train_dirs(root)
            assert resolved is not None
            images, labels = resolved
            self.assertEqual(images, root / "images")
            self.assertEqual(labels, root / "labels")

    def test_missing_returns_none(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            self.assertIsNone(resolve_self_train_dirs(root))

    def test_partial_train_tree_not_flat(self) -> None:
        """images/train exists but labels/train missing → not flat fallback."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images" / "train").mkdir(parents=True)
            (root / "labels").mkdir()
            self.assertIsNone(resolve_self_train_dirs(root))


class RankUncertainQueueSoftTests(unittest.TestCase):
    def test_empty_queue_returns_none(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            queue = root / "uncertain_queue.jsonl"
            queue.write_text("", encoding="utf-8")
            progress = root / "progress"
            self.assertIsNone(rank_uncertain_queue_soft(queue, progress))
            self.assertFalse((progress / "uncertain_review.json").exists())

    def test_missing_queue_returns_none(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertIsNone(rank_uncertain_queue_soft(root / "missing.jsonl", root / "progress"))

    def test_ranks_queue(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            queue = root / "uncertain_queue.jsonl"
            queue.write_text(
                "\n".join(
                    [
                        json.dumps({"stem": "hard", "max_conf": 0.2}),
                        json.dumps({"stem": "easy", "max_conf": 0.9}),
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            progress = root / "progress"
            out = rank_uncertain_queue_soft(queue, progress, top_k=10)
            assert out is not None
            self.assertTrue(out.is_file())
            self.assertTrue((progress / "uncertain_review.jsonl").is_file())
            payload = json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(payload[0]["stem"], "hard")


if __name__ == "__main__":
    unittest.main()
