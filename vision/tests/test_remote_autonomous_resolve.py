"""Tests for remote autonomous dataset zip resolve and held-out splits."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

from cs2_vision_access.training.remote_autonomous import (
    AutonomousReport,
    _rank_and_write_uncertain_review,
    materialize_held_out_split,
    promote_cs2_10k_holdout_to_session_split,
    resolve_remote_dataset_zip,
    run_autonomous_loop,
)
from cs2_vision_access.training.self_train import _label_is_pseudo_or_empty
from tests.remote_autonomous_helpers import AutonomousLoopFakesMixin


class ResolveZipTests(unittest.TestCase):
    def test_explicit_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            z = Path(tmp) / "d.zip"
            z.write_bytes(b"PK\x03\x04")
            self.assertEqual(resolve_remote_dataset_zip(z), z)

    def test_missing_returns_none_or_found(self) -> None:
        # No crash when nothing exists
        result = resolve_remote_dataset_zip(
            None, search_roots=[tempfile.gettempdir() + "/no-such-cs2-zip-root"]
        )
        self.assertTrue(result is None or result.suffix == ".zip")


class HeldOutSplitTests(unittest.TestCase):
    def test_same_seed_produces_the_same_split_plan(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            for index in range(10):
                stem = f"frame_{index:08d}"
                (images / f"{stem}.jpg").write_bytes(b"not-decoded-by-split")
                (labels / f"{stem}.txt").write_text("", encoding="utf-8")
            first = materialize_held_out_split(root, val_fraction=0.3, seed=73)
            for split_name in ("train", "val"):
                for path in (images / split_name).glob("*"):
                    path.unlink()
                for path in (labels / split_name).glob("*"):
                    path.unlink()
            second = materialize_held_out_split(root, val_fraction=0.3, seed=73)
            self.assertEqual(first, second)

    def test_materialize_held_out_split(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            import cv2

            for i in range(5):
                cv2.imwrite(
                    str(images / f"frame_{i:08d}.jpg"),
                    np.zeros((32, 32, 3), dtype=np.uint8),
                )
                (labels / f"frame_{i:08d}.txt").write_text(
                    "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n", encoding="utf-8"
                )
            plan = materialize_held_out_split(root, val_fraction=0.2, seed=42)
            self.assertGreaterEqual(len(plan["val"]), 1)
            self.assertGreaterEqual(len(plan["train"]), 1)
            self.assertTrue((images / "train").is_dir())
            self.assertTrue((images / "val").is_dir())
            self.assertTrue((labels / "train").is_dir())
            self.assertTrue((labels / "val").is_dir())
            self.assertGreater(len(list((images / "train").glob("*.jpg"))), 0)
            self.assertGreater(len(list((images / "val").glob("*.jpg"))), 0)

    def test_materialize_propagates_pseudo_sidecars(self) -> None:
        """Held-out split must hardlink/copy labels/*.txt.pseudo into train/val."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            import cv2

            for i in range(5):
                cv2.imwrite(
                    str(images / f"frame_{i:08d}.jpg"),
                    np.zeros((32, 32, 3), dtype=np.uint8),
                )
                lab = labels / f"frame_{i:08d}.txt"
                lab.write_text("0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n", encoding="utf-8")
                # Mark all as revisable pseudo via Ultralytics-safe sidecar.
                (labels / f"frame_{i:08d}.txt.pseudo").write_text("", encoding="utf-8")

            plan = materialize_held_out_split(root, val_fraction=0.2, seed=42)
            train_stem = plan["train"][0]
            train_label = labels / "train" / f"{train_stem}.txt"
            train_sidecar = labels / "train" / f"{train_stem}.txt.pseudo"
            self.assertTrue(train_label.is_file(), train_label)
            self.assertTrue(
                train_sidecar.is_file(),
                f"expected pseudo sidecar for train stem {train_stem}",
            )
            self.assertTrue(
                _label_is_pseudo_or_empty(train_label),
                "train label with .pseudo sidecar must be revisable",
            )
            # Val side also gets sidecars when present.
            val_stem = plan["val"][0]
            val_sidecar = labels / "val" / f"{val_stem}.txt.pseudo"
            self.assertTrue(val_sidecar.is_file(), val_sidecar)


class Cs210kHoldoutPromoteTests(AutonomousLoopFakesMixin, unittest.TestCase):
    def test_promote_holdout_never_in_train(self) -> None:
        """images_val + holdout JSON → session_split; holdout stems stay out of train."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images_val = root / "images_val"
            images.mkdir()
            labels.mkdir()
            images_val.mkdir()
            import cv2

            # Train-pool frames under images/
            for i in range(4):
                stem = f"frame_{i:08d}"
                cv2.imwrite(
                    str(images / f"{stem}.jpg"),
                    np.zeros((32, 32, 3), dtype=np.uint8),
                )
                (labels / f"{stem}.txt").write_text(
                    "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n", encoding="utf-8"
                )
            # Holdout frames under images_val/
            holdout_stems = ["frame_00000010", "frame_00000011"]
            for stem in holdout_stems:
                cv2.imwrite(
                    str(images_val / f"{stem}.jpg"),
                    np.full((32, 32, 3), 80, dtype=np.uint8),
                )
                (labels / f"{stem}.txt").write_text(
                    "0 0.2 0.2 0.8 0.2 0.8 0.8 0.2 0.8\n", encoding="utf-8"
                )
            (root / "cs2_10k_holdout_videos.json").write_text(
                json.dumps(
                    {
                        "holdout_videos": ["clip-hold"],
                        "holdout_frames": holdout_stems,
                        "holdout_video_fraction": 0.2,
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )

            plan = promote_cs2_10k_holdout_to_session_split(root)
            self.assertIsNotNone(plan)
            assert plan is not None
            self.assertEqual(set(plan["val"]), set(holdout_stems))
            self.assertTrue(plan["train"])
            # Deterministic: holdout never appears in train.
            self.assertFalse(set(holdout_stems) & set(plan["train"]))
            train_names = {p.stem for p in (images / "train").glob("*.jpg")}
            val_names = {p.stem for p in (images / "val").glob("*.jpg")}
            self.assertEqual(val_names, set(holdout_stems))
            self.assertFalse(set(holdout_stems) & train_names)
            # Labels follow stems.
            for stem in holdout_stems:
                self.assertTrue((labels / "val" / f"{stem}.txt").is_file())
            for stem in plan["train"]:
                self.assertTrue((labels / "train" / f"{stem}.txt").is_file())
            # Plan file records provenance.
            plan_path = root / "held_out_split.json"
            self.assertTrue(plan_path.is_file())
            payload = json.loads(plan_path.read_text(encoding="utf-8"))
            self.assertEqual(payload.get("source"), "cs2_10k_video_holdout")
            self.assertEqual(set(payload["val"]), set(holdout_stems))

    def test_promote_returns_none_without_holdout(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            import cv2

            cv2.imwrite(
                str(images / "frame_00000000.jpg"),
                np.zeros((16, 16, 3), dtype=np.uint8),
            )
            self.assertIsNone(promote_cs2_10k_holdout_to_session_split(root))

    def test_loop_skips_random_split_when_cs2_holdout_present(self) -> None:
        """run_autonomous_loop with staged holdout uses video holdout as val."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            images = data / "images"
            labels = data / "labels"
            images_val = data / "images_val"
            images.mkdir(parents=True)
            labels.mkdir()
            images_val.mkdir()
            import cv2

            for i in range(5):
                cv2.imwrite(
                    str(images / f"frame_{i:08d}.jpg"),
                    np.zeros((32, 32, 3), dtype=np.uint8),
                )
                (labels / f"frame_{i:08d}.txt").write_text(
                    "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n", encoding="utf-8"
                )
            holdout = ["frame_00000090", "frame_00000091"]
            for stem in holdout:
                cv2.imwrite(
                    str(images_val / f"{stem}.jpg"),
                    np.full((32, 32, 3), 40, dtype=np.uint8),
                )
                (labels / f"{stem}.txt").write_text(
                    "0 0.2 0.2 0.8 0.2 0.8 0.8 0.2 0.8\n", encoding="utf-8"
                )
            (data / "cs2_10k_holdout_videos.json").write_text(
                json.dumps({"holdout_frames": holdout, "holdout_videos": ["v"]}) + "\n",
                encoding="utf-8",
            )

            suite = self
            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=suite._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=suite._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=suite._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=suite._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                    allow_leaky_val=False,
                    use_cs2_10k=False,
                )

            self.assertIsInstance(report, AutonomousReport)
            val_names = {p.stem for p in (images / "val").glob("*.jpg")}
            train_names = {p.stem for p in (images / "train").glob("*.jpg")}
            self.assertEqual(val_names, set(holdout))
            self.assertFalse(set(holdout) & train_names)
            self.assertTrue(
                any("video holdout" in n for n in report.notes),
                report.notes,
            )
            plan = json.loads((data / "held_out_split.json").read_text(encoding="utf-8"))
            self.assertEqual(plan.get("source"), "cs2_10k_video_holdout")


class UncertainReviewRankTests(unittest.TestCase):
    def test_rank_writes_progress_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp)
            progress = data / "progress"
            queue = data / "uncertain_queue.jsonl"
            queue.write_text(
                "\n".join(
                    [
                        json.dumps(
                            {
                                "stem": "hard",
                                "max_conf": 0.25,
                                "path": "images/hard.jpg",
                            }
                        ),
                        json.dumps(
                            {
                                "stem": "easy",
                                "max_conf": 0.9,
                                "path": "images/easy.jpg",
                            }
                        ),
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            notes: list[str] = []
            n = _rank_and_write_uncertain_review(data, progress, top_k=10, notes=notes)
            self.assertEqual(n, 2)
            self.assertEqual(notes, [])
            review = progress / "uncertain_review.json"
            review_l = progress / "uncertain_review.jsonl"
            self.assertTrue(review.is_file())
            self.assertTrue(review_l.is_file())
            payload = json.loads(review.read_text(encoding="utf-8"))
            self.assertEqual(payload[0]["stem"], "hard")

    def test_missing_queue_is_noop(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp)
            progress = data / "progress"
            n = _rank_and_write_uncertain_review(data, progress)
            self.assertIsNone(n)
            self.assertFalse((progress / "uncertain_review.json").exists())

    def test_rank_failure_is_soft(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp)
            progress = data / "progress"
            # Invalid JSON line still shouldn't raise via helper.
            (data / "uncertain_queue.jsonl").write_text("{not-json\n", encoding="utf-8")
            notes: list[str] = []
            n = _rank_and_write_uncertain_review(data, progress, notes=notes)
            # Malformed-only file yields empty ranked list (not an error).
            self.assertEqual(n, 0)
            self.assertEqual(notes, [])


if __name__ == "__main__":
    unittest.main()
