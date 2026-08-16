"""Tests for the download-model CLI handler.

These tests verify the command registration, argument parsing, model listing,
and helper functions. Actual network downloads and ONNX exports are mocked.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.cli.parser import build_parser


class _FakeResponse:
    def __init__(self, body: bytes, *, url: str, content_length: int | None = None) -> None:
        self._body = body
        self._offset = 0
        self._url = url
        self.headers: dict[str, str] = {}
        if content_length is not None:
            self.headers["Content-Length"] = str(content_length)

    def __enter__(self) -> _FakeResponse:
        return self

    def __exit__(self, *_args: object) -> None:
        return None

    def geturl(self) -> str:
        return self._url

    def read(self, size: int) -> bytes:
        chunk = self._body[self._offset : self._offset + size]
        self._offset += len(chunk)
        return chunk


class _FakeOpener:
    def __init__(self, response: _FakeResponse) -> None:
        self._response = response

    def open(self, _request: object, *, timeout: int) -> _FakeResponse:
        del timeout
        return self._response


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

    def test_overlap_uses_pinned_registry_artifact(self) -> None:
        from cs2_vision_access.cli.handlers import download_model

        args = build_parser().parse_args(["download-model", "yolo11n-seg"])
        with (
            patch.object(download_model, "_handle_onnx_direct", return_value=0) as direct,
            patch.object(download_model, "_handle_pt_download") as checkpoint,
        ):
            self.assertEqual(download_model._handle_download_model(args), 0)
        direct.assert_called_once()
        checkpoint.assert_not_called()


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


class TestHttpsDownloadValidation(unittest.TestCase):
    """Reject unsafe model artifact URLs before opening a downloader."""

    @staticmethod
    def _arguments(output_dir: Path) -> argparse.Namespace:
        return argparse.Namespace(
            output_dir=output_dir,
            image_size=640,
            device="cpu",
            overwrite=False,
            classes=None,
            origin=None,
            license=None,
        )

    def test_validator_rejects_unsafe_schemes_hosts_and_credentials(self) -> None:
        from cs2_vision_access.cli.handlers.download_model import (
            DownloadModelError,
            _require_https_download_url,
        )

        unsafe_urls = (
            "",
            "//example.test/model.onnx",
            "http://example.test/model.onnx",
            "ftp://example.test/model.onnx",
            "file:///tmp/model.onnx",
            "s3://bucket/model.onnx",
            "https://",
            "https:///model.onnx",
            "https://user@example.test/model.onnx",
            "https://:password@example.test/model.onnx",
        )
        for url in unsafe_urls:
            with self.subTest(url=url):
                with self.assertRaisesRegex(DownloadModelError, "invalid HTTPS download URL"):
                    _require_https_download_url(url)

    def test_all_cli_download_paths_reject_invalid_urls_before_downloading(self) -> None:
        from cs2_vision_access.cli.handlers import download_model

        with tempfile.TemporaryDirectory() as tmp:
            arguments = self._arguments(Path(tmp))
            unsafe_url = "https://user@example.test/model.onnx"
            cases = (
                (
                    lambda: download_model._handle_pt_download("unsafe", arguments),
                    {"unsafe": {"pt_url": unsafe_url, "sha256": "0" * 64}},
                ),
                (
                    lambda: download_model._handle_registry_pt(
                        "unsafe",
                        {"url": unsafe_url, "sha256": "0" * 64},
                        arguments,
                    ),
                    None,
                ),
                (
                    lambda: download_model._handle_onnx_direct(
                        "unsafe",
                        {"url": unsafe_url, "format": "onnx", "sha256": "0" * 64},
                        arguments,
                    ),
                    None,
                ),
            )
            for run, downloadable_models in cases:
                with self.subTest(run=run):
                    with patch.object(download_model, "download_verified_https") as download:
                        if downloadable_models is None:
                            with self.assertRaisesRegex(
                                download_model.DownloadModelError,
                                "invalid HTTPS download URL",
                            ):
                                run()
                        else:
                            with (
                                patch.object(
                                    download_model,
                                    "_DOWNLOADABLE_MODELS",
                                    downloadable_models,
                                ),
                                self.assertRaisesRegex(
                                    download_model.DownloadModelError,
                                    "invalid HTTPS download URL",
                                ),
                            ):
                                run()
                    download.assert_not_called()

    def test_unpinned_downloadable_model_fails_before_network_or_model_load(self) -> None:
        from cs2_vision_access.cli.handlers import download_model

        with tempfile.TemporaryDirectory() as tmp:
            with (
                patch.object(download_model, "download_verified_https") as download,
                self.assertRaisesRegex(download_model.DownloadModelError, "registry SHA-256 pin"),
            ):
                download_model._handle_pt_download("yolo26n-seg", self._arguments(Path(tmp)))
        download.assert_not_called()


class TestPinnedDownloadAdversarialCases(unittest.TestCase):
    """Pinned downloads must not promote attacker-controlled or incomplete bytes."""

    @staticmethod
    def _arguments(output_dir: Path) -> argparse.Namespace:
        return TestHttpsDownloadValidation._arguments(output_dir)

    def _run_with_response(
        self,
        root: Path,
        response: _FakeResponse,
        *,
        expected: bytes,
        message: str,
    ) -> None:
        from cs2_vision_access.cli.handlers import download_model

        registry_info = {
            "format": "onnx",
            "url": "https://example.test/model.onnx",
            "sha256": hashlib.sha256(expected).hexdigest(),
            "classes": {"0": "player"},
        }
        with (
            patch(
                "cs2_vision_access.cli.handlers._model_ops.urllib.request.build_opener",
                return_value=_FakeOpener(response),
            ),
            patch.object(download_model, "_create_manifest") as create_manifest,
            self.assertRaisesRegex(download_model.DownloadModelError, message),
        ):
            download_model._handle_onnx_direct(
                "pinned-test", registry_info, self._arguments(root)
            )
        create_manifest.assert_not_called()
        self.assertFalse((root / "model.onnx").exists())
        self.assertEqual(list(root.glob(".model.onnx.*.download")), [])

    def test_digest_mismatch_is_not_promoted_or_loaded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self._run_with_response(
                Path(tmp),
                _FakeResponse(b"attacker bytes", url="https://example.test/model.onnx"),
                expected=b"trusted bytes",
                message="SHA-256 mismatch",
            )

    def test_truncated_payload_is_not_promoted_or_loaded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self._run_with_response(
                Path(tmp),
                _FakeResponse(b"abc", url="https://example.test/model.onnx", content_length=4),
                expected=b"abc",
                message="truncated",
            )

    def test_redirect_to_unapproved_host_is_not_promoted_or_loaded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self._run_with_response(
                Path(tmp),
                _FakeResponse(b"trusted bytes", url="https://attacker.test/model.onnx"),
                expected=b"trusted bytes",
                message="unapproved host",
            )

    def test_existing_alias_must_match_the_registry_pin(self) -> None:
        from cs2_vision_access.cli.handlers import download_model

        trusted = b"trusted bytes"
        registry_info = {
            "format": "onnx",
            "url": "https://example.test/model.onnx",
            "sha256": hashlib.sha256(trusted).hexdigest(),
            "classes": {"0": "player"},
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "model.onnx").write_bytes(trusted)
            alias = root / "pinned-test.onnx"
            alias.write_bytes(b"attacker bytes")
            with self.assertRaisesRegex(download_model.DownloadModelError, "SHA-256 mismatch"):
                download_model._handle_onnx_direct(
                    "pinned-test", registry_info, self._arguments(root)
                )
            self.assertFalse(alias.exists())


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
