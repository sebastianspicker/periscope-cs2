"""Tests for training CLI helpers: leaky val, prepare, batch, bundle."""

from __future__ import annotations

import tempfile
import unittest
import zipfile
from pathlib import Path


class TrainLeakyValTests(unittest.TestCase):
    """Pure leaky-val helpers on training.train (no Ultralytics required)."""

    def test_train_val_paths_are_leaky_same(self) -> None:
        from cs2_vision_access.training.train import train_val_paths_are_leaky

        self.assertTrue(train_val_paths_are_leaky("images", "images"))
        self.assertTrue(train_val_paths_are_leaky("images/", "images"))
        self.assertTrue(train_val_paths_are_leaky(r"images\train", "images/train"))

    def test_train_val_paths_are_leaky_disjoint(self) -> None:
        from cs2_vision_access.training.train import train_val_paths_are_leaky

        self.assertFalse(train_val_paths_are_leaky("images/train", "images/val"))
        self.assertFalse(train_val_paths_are_leaky(None, "images"))
        self.assertFalse(train_val_paths_are_leaky("", "images"))

    def test_train_val_paths_are_leaky_resolves_via_yaml_parent(self) -> None:
        from cs2_vision_access.training.train import train_val_paths_are_leaky

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            yaml_path = root / "dataset.yaml"
            yaml_path.write_text("train: images\nval: images\n", encoding="utf-8")
            # Relative forms that resolve to the same directory under yaml parent.
            self.assertTrue(
                train_val_paths_are_leaky(
                    "images",
                    str((root / "images").resolve()),
                    dataset_yaml_path=yaml_path,
                )
            )
            self.assertFalse(
                train_val_paths_are_leaky(
                    "images/train",
                    "images/val",
                    dataset_yaml_path=yaml_path,
                )
            )

    def test_check_leaky_val_refuses_same_path(self) -> None:
        from cs2_vision_access.training.train import (
            LeakyValidationError,
            check_leaky_val,
        )

        with tempfile.TemporaryDirectory() as tmp:
            yaml_path = Path(tmp) / "dataset.yaml"
            yaml_path.write_text(
                "path: .\ntrain: images\nval: images\nnc: 4\n",
                encoding="utf-8",
            )
            with self.assertRaises(LeakyValidationError):
                check_leaky_val(yaml_path, allow_leaky_val=False)

    def test_check_leaky_val_allows_with_flag(self) -> None:
        from cs2_vision_access.training.train import check_leaky_val

        with tempfile.TemporaryDirectory() as tmp:
            yaml_path = Path(tmp) / "dataset.yaml"
            yaml_path.write_text(
                "path: .\ntrain: images\nval: images\nnc: 4\n",
                encoding="utf-8",
            )
            check_leaky_val(yaml_path, allow_leaky_val=True)  # no raise

    def test_check_leaky_val_ok_when_split(self) -> None:
        from cs2_vision_access.training.train import check_leaky_val

        with tempfile.TemporaryDirectory() as tmp:
            yaml_path = Path(tmp) / "dataset.yaml"
            yaml_path.write_text(
                "path: .\ntrain: images/train\nval: images/val\nnc: 1\n",
                encoding="utf-8",
            )
            check_leaky_val(yaml_path, allow_leaky_val=False)

    def test_parser_defaults_match_self_train_profile(self) -> None:
        from cs2_vision_access.training.contracts import PROFILES, VOMBIT_CLASSES
        from cs2_vision_access.training.train import _build_parser

        profile = PROFILES["self_train"]
        parser = _build_parser()
        args = parser.parse_args(["--data", "dummy.yaml"])
        self.assertEqual(args.epochs, profile.epochs)
        self.assertEqual(args.batch, profile.batch)
        self.assertEqual(args.imgsz, profile.image_size)
        self.assertEqual(args.model, profile.base_model)
        self.assertFalse(args.allow_leaky_val)
        self.assertEqual(
            list(VOMBIT_CLASSES.values()),
            ["ct", "ct_head", "t", "t_head"],
        )


class TrainingPrepareCliTests(unittest.TestCase):
    def test_prepare_parser_accepts_video_source(self) -> None:
        from cs2_vision_access.training.prepare import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--video",
                "test.mp4",
                "--detector",
                "det.onnx",
                "--manifest",
                "model.json",
                "--encoder",
                "enc.onnx",
                "--decoder",
                "dec.onnx",
                "--output",
                "data/out",
            ]
        )
        self.assertEqual(args.video, "test.mp4")
        self.assertEqual(args.detector, "det.onnx")
        self.assertEqual(args.output, "data/out")

    def test_prepare_parser_parses_video_without_rejecting(self) -> None:
        from cs2_vision_access.training.prepare import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--video",
                "test.mp4",
                "--detector",
                "det.onnx",
                "--manifest",
                "model.json",
                "--encoder",
                "enc.onnx",
                "--decoder",
                "dec.onnx",
            ]
        )
        self.assertEqual(args.video, "test.mp4")
        self.assertFalse(args.tar)
        self.assertFalse(args.live_screen)

    def test_prepare_parser_default_output(self) -> None:
        from cs2_vision_access.training.prepare import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--video",
                "test.mp4",
                "--detector",
                "det.onnx",
                "--manifest",
                "model.json",
                "--encoder",
                "enc.onnx",
                "--decoder",
                "dec.onnx",
            ]
        )
        self.assertEqual(args.output, "data/cs2_train")


class TrainingBatchCliTests(unittest.TestCase):
    def test_batch_parser_accepts_multiple_sources(self) -> None:
        from cs2_vision_access.training.batch import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--sources",
                "a.mp4",
                "b.mp4",
                "--detector",
                "det.onnx",
                "--manifest",
                "model.json",
                "--encoder",
                "enc.onnx",
                "--decoder",
                "dec.onnx",
            ]
        )
        self.assertEqual(args.sources, ["a.mp4", "b.mp4"])
        self.assertEqual(args.output, "data/cs2_train")
        self.assertEqual(args.sample_rate, 2.0)
        self.assertEqual(args.max_frames_per_source, 300)


class BundleDatasetTests(unittest.TestCase):
    def test_bundle_packs_jpg_and_png(self) -> None:
        from cs2_vision_access.training.bundle import create_bundle

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            (root / "images" / "a.jpg").write_bytes(b"jpg")
            (root / "images" / "b.png").write_bytes(b"png")
            (root / "images" / "c.jpeg").write_bytes(b"jpeg")
            for stem in ("a", "b", "c"):
                (root / "labels" / f"{stem}.txt").write_text(
                    "0 0.5 0.5 0.1 0.1\n", encoding="utf-8"
                )
            out = root / "nested" / "bundle.zip"
            count, path = create_bundle(root, out)
            self.assertEqual(count, 3)
            self.assertTrue(path.is_file())
            with zipfile.ZipFile(path, "r") as zf:
                names = set(zf.namelist())
            self.assertIn("dataset.yaml", names)
            self.assertIn("images/a.jpg", names)
            self.assertIn("images/b.png", names)
            self.assertIn("images/c.jpeg", names)
            yaml_text = zipfile.ZipFile(path).read("dataset.yaml").decode("utf-8")
            self.assertIn("path: .", yaml_text)
            self.assertIn("ct_head", yaml_text)

    def test_bundle_skips_missing_images(self) -> None:
        from cs2_vision_access.training.bundle import create_bundle

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            (root / "images" / "keep.jpg").write_bytes(b"x")
            (root / "labels" / "keep.txt").write_text("0 0.5 0.5 0.1 0.1\n")
            (root / "labels" / "orphan.txt").write_text("0 0.5 0.5 0.1 0.1\n")
            count, path = create_bundle(root, root / "b.zip")
            self.assertEqual(count, 1)
            with zipfile.ZipFile(path, "r") as zf:
                self.assertEqual(
                    {n for n in zf.namelist() if n.startswith("labels/")},
                    {"labels/keep.txt"},
                )

    def test_bundle_rejects_empty(self) -> None:
        from cs2_vision_access.training.bundle import create_bundle

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            (root / "labels" / "only.txt").write_text("0 0.5 0.5 0.1 0.1\n")
            with self.assertRaises(ValueError):
                create_bundle(root, root / "empty.zip")

    def test_bundle_parser_accepts_names(self) -> None:
        from cs2_vision_access.training.bundle import DEFAULT_NAMES, _build_parser

        parser = _build_parser()
        args = parser.parse_args(["--input", "data/x", "--output", "out.zip", "--names", "a", "b"])
        self.assertEqual(args.names, ["a", "b"])
        self.assertEqual(DEFAULT_NAMES, ["ct", "ct_head", "t", "t_head"])


if __name__ == "__main__":
    unittest.main()
