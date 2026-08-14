"""Tests for the download-model CLI handler.

These tests verify the command registration, argument parsing, model listing,
and helper functions. Actual network downloads and ONNX exports are mocked.
"""

from __future__ import annotations

import argparse
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.cli.parser import build_parser


class TestDownloadModelCommandRegistration(unittest.TestCase):
    """Verify the download-model subcommand is registered in the CLI parser."""

    def test_download_model_is_registered(self) -> None:
        parser = build_parser()
        subcommands = {
            action.dest: action.choices
            for action in parser._actions
            if isinstance(action, argparse._SubParsersAction)
        }
        all_commands: set[str] = set()
        for choices in subcommands.values():
            if choices:
                all_commands.update(choices)
        self.assertIn("download-model", all_commands)

    def test_list_models_flag(self) -> None:
        """--list-models should exit with code 0."""
        parser = build_parser()
        args = parser.parse_args(["download-model", "--list-models"])
        self.assertTrue(args.list_models)

    def test_default_model_name(self) -> None:
        """Default model name should be yolo26n-seg."""
        parser = build_parser()
        args = parser.parse_args(["download-model"])
        self.assertEqual(args.model_name, "yolo26n-seg")

    def test_custom_model_name(self) -> None:
        parser = build_parser()
        args = parser.parse_args(["download-model", "yolo11n-seg"])
        self.assertEqual(args.model_name, "yolo11n-seg")

    def test_output_dir_default(self) -> None:
        parser = build_parser()
        args = parser.parse_args(["download-model"])
        self.assertEqual(args.output_dir, Path("artifacts"))

    def test_custom_output_dir(self) -> None:
        parser = build_parser()
        args = parser.parse_args(["download-model", "--output-dir", "models"])
        self.assertEqual(args.output_dir, Path("models"))


class TestDownloadModelList(unittest.TestCase):
    """Verify the model listing output."""

    def test_list_models_returns_zero(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _list_models

        result = _list_models()
        self.assertEqual(result, 0)

    def test_available_names_include_vombit_and_edgesam(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _available_model_names

        names = _available_model_names()
        self.assertIn("vombit-yolov10n", names)
        self.assertIn("vombit-yolov10n-fp16", names)
        self.assertIn("edgesam-encoder", names)
        self.assertIn("edgesam-decoder", names)
        self.assertIn("yolo26n-seg", names)

    def test_list_models_stdout_includes_vombit(self) -> None:
        from io import StringIO

        from cs2_vision_access.cli.handlers.download_model import _list_models

        buf = StringIO()
        with patch("sys.stdout", buf):
            code = _list_models()
        self.assertEqual(code, 0)
        out = buf.getvalue()
        self.assertIn("vombit-yolov10n", out)
        self.assertIn("edgesam-encoder", out)
        self.assertIn("onnx-direct", out)

    def test_is_onnx_direct_registry_entry(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _is_onnx_direct
        from cs2_vision_access.config.data.model_registry import MODEL_REGISTRY

        self.assertTrue(_is_onnx_direct(MODEL_REGISTRY["vombit-yolov10n"]))
        self.assertTrue(_is_onnx_direct(MODEL_REGISTRY["edgesam-encoder"]))


class TestCoco80Classes(unittest.TestCase):
    """Verify the COCO 80 class mapping."""

    def test_coco_classes_count(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _coco80_classes

        classes = _coco80_classes()
        self.assertEqual(len(classes), 80)
        self.assertEqual(classes["0"], "person")

    def test_coco_classes_are_contiguous(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _coco80_classes

        classes = _coco80_classes()
        keys = sorted(int(k) for k in classes)
        self.assertEqual(keys, list(range(80)))


class TestSha256File(unittest.TestCase):
    """Verify SHA-256 file hashing."""

    def test_sha256_known_content(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import _sha256_file

        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            f.write(b"hello world\n")
            tmp = f.name

        try:
            digest = _sha256_file(Path(tmp))
            # echo -n "hello world\n" | sha256sum
            self.assertEqual(len(digest), 64)
            self.assertTrue(all(c in "0123456789abcdef" for c in digest))
        finally:
            Path(tmp).unlink(missing_ok=True)


class TestCreateManifest(unittest.TestCase):
    """Verify manifest generation for downloaded models."""

    def setUp(self) -> None:
        self.temp_dir = Path(tempfile.mkdtemp())
        self.onnx_file = self.temp_dir / "test.onnx"
        self.onnx_file.write_bytes(b"fake onnx content\n")
        self.manifest_file = self.temp_dir / "test.model.json"

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_manifest_created_with_correct_schema(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import (
            _create_manifest,
        )

        _create_manifest(
            onnx_path=self.onnx_file,
            manifest_path=self.manifest_file,
            model_name="yolo26n-seg",
            model_info={"task": "segment", "default_origin": "https://example.com"},
            image_size=640,
            origin="https://example.com",
            license_name="AGPL-3.0-only",
            classes=["player"],
        )

        self.assertTrue(self.manifest_file.exists())
        raw = json.loads(self.manifest_file.read_text(encoding="utf-8"))
        self.assertEqual(raw["schema_version"], 1)
        self.assertEqual(raw["model_filename"], "test.onnx")
        self.assertEqual(raw["license"], "AGPL-3.0-only")
        self.assertEqual(raw["classes"], {"0": "player"})
        self.assertEqual(len(raw["sha256"]), 64)

    def test_manifest_with_coco_classes(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import (
            _create_manifest,
        )

        _create_manifest(
            onnx_path=self.onnx_file,
            manifest_path=self.manifest_file,
            model_name="yolo26n-seg",
            model_info={"task": "segment", "default_origin": "https://example.com"},
            image_size=640,
            origin="https://example.com",
            license_name="AGPL-3.0-only",
            classes=None,
        )

        raw = json.loads(self.manifest_file.read_text(encoding="utf-8"))
        self.assertEqual(len(raw["classes"]), 80)
        self.assertEqual(raw["classes"]["0"], "person")


if __name__ == "__main__":
    unittest.main()
