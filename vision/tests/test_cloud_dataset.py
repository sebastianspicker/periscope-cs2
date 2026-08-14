"""Tests for dataset zip helpers and cloud extract/manifest."""

from __future__ import annotations

import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from cs2_vision_access.training.cloud import (
    DEFAULT_CLASSES,
    create_manifest,
    extract_dataset,
)
from cs2_vision_access.training.dataset_zip import (
    count_images,
    count_labels,
    is_unsafe_zip_member,
    resolve_dataset_root,
    safe_extract_zip,
)
from tests.cloud_test_helpers import _write_minimal_zip


class DatasetZipHelpersTests(unittest.TestCase):
    """Pure helpers in training.dataset_zip (used by cloud extract path)."""

    def test_is_unsafe_zip_member(self) -> None:
        self.assertTrue(is_unsafe_zip_member("../escape.txt"))
        self.assertTrue(is_unsafe_zip_member("/tmp/evil.txt"))
        self.assertTrue(is_unsafe_zip_member("C:/windows/evil.txt"))
        self.assertFalse(is_unsafe_zip_member("images/ok.jpg"))
        self.assertFalse(is_unsafe_zip_member("nested/labels/x.txt"))

    def test_count_images_and_labels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "a.JPG").write_bytes(b"x")
            (images / "b.png").write_bytes(b"x")
            (images / "skip.txt").write_text("nope")
            (labels / "a.txt").write_text("0 0.5 0.5 0.1 0.1\n")
            (labels / "b.txt").write_text("0 0.5 0.5 0.1 0.1\n")
            self.assertEqual(count_images(images), 2)
            self.assertEqual(count_labels(labels), 2)
            self.assertEqual(count_images(root / "missing"), 0)

    def test_resolve_dataset_root_nested(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            nested = root / "cs2_bundle"
            (nested / "images").mkdir(parents=True)
            (nested / "labels").mkdir(parents=True)
            self.assertEqual(resolve_dataset_root(root), nested)

    def test_safe_extract_zip_rejects_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "evil.zip"
            out = root / "out"
            out.mkdir()
            with zipfile.ZipFile(zip_path, "w") as zf:
                zf.writestr("../escape.txt", b"nope")
            with zipfile.ZipFile(zip_path, "r") as zf, self.assertRaises(ValueError):
                safe_extract_zip(zf, out)


class CloudExtractDatasetTests(unittest.TestCase):
    def test_extract_dataset_missing_zip_raises(self) -> None:
        missing = Path(tempfile.gettempdir()) / "nonexistent_file.zip"
        with self.assertRaises(FileNotFoundError):
            extract_dataset(missing, output_dir=tempfile.gettempdir())

    def test_extract_flat_jpg_and_png(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "data.zip"
            out = root / "out"
            _write_minimal_zip(
                zip_path,
                images=["a.JPG", "b.png", "c.jpeg"],
            )
            data_root = extract_dataset(zip_path, output_dir=out)
            self.assertEqual(data_root, out)
            self.assertTrue((data_root / "images" / "a.JPG").is_file())
            self.assertTrue((data_root / "images" / "b.png").is_file())
            self.assertTrue((data_root / "images" / "c.jpeg").is_file())
            self.assertTrue((data_root / "labels" / "a.txt").is_file())

    def test_extract_nested_root(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "nested.zip"
            out = root / "out"
            _write_minimal_zip(zip_path, nested_root="cs2_bundle", images=["x.jpg"])
            data_root = extract_dataset(zip_path, output_dir=out)
            self.assertEqual(data_root, out / "cs2_bundle")
            self.assertTrue((data_root / "images" / "x.jpg").is_file())
            self.assertTrue((data_root / "labels" / "x.txt").is_file())

    def test_extract_rejects_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "evil.zip"
            out = root / "out"
            with zipfile.ZipFile(zip_path, "w") as zf:
                zf.writestr("../escape.txt", b"nope")
                zf.writestr("images/ok.jpg", b"img")
                zf.writestr("labels/ok.txt", "0 0.5 0.5 0.1 0.1\n")
            with self.assertRaises(ValueError) as ctx:
                extract_dataset(zip_path, output_dir=out)
            self.assertIn("unsafe", str(ctx.exception).lower())

    def test_extract_rejects_absolute_member(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "abs.zip"
            out = root / "out"
            with zipfile.ZipFile(zip_path, "w") as zf:
                # Forward-slash absolute path (common zip-slip variant)
                zf.writestr("/tmp/evil.txt", b"nope")
            with self.assertRaises(ValueError):
                extract_dataset(zip_path, output_dir=out)


class CloudCreateManifestTests(unittest.TestCase):
    def test_create_manifest_creates_json(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            onnx_path = Path(tmp) / "model.onnx"
            onnx_path.write_bytes(b"fake_model_bytes")
            data_dir = Path(tmp) / "data"
            data_dir.mkdir()
            manifest_path = create_manifest(onnx_path, data_dir)
            self.assertEqual(
                manifest_path,
                data_dir / "cs2-yolo11n-seg.model.json",
            )
            self.assertTrue(manifest_path.is_file())
            payload = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(
                payload["classes"],
                {
                    "0": "ct",
                    "1": "ct_head",
                    "2": "t",
                    "3": "t_head",
                },
            )
            self.assertEqual(payload["classes"], {str(k): v for k, v in DEFAULT_CLASSES.items()})

    def test_create_manifest_custom_classes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            onnx_path = Path(tmp) / "model.onnx"
            onnx_path.write_bytes(b"bytes")
            data_dir = Path(tmp) / "data"
            data_dir.mkdir()
            custom = {0: "player", 1: "corpse"}
            manifest_path = create_manifest(onnx_path, data_dir, classes=custom)
            payload = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(payload["classes"], {"0": "player", "1": "corpse"})


if __name__ == "__main__":
    unittest.main()
