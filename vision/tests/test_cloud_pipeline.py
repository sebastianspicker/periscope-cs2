"""Tests for cloud pipeline helpers, OOM retry, and install deps."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.training.cloud import (
    DEFAULT_BATCH,
    DEFAULT_CLASSES,
    _retry_batch_on_oom,
    find_dataset_zip,
    package_outputs,
)
from tests.cloud_test_helpers import _CLOUD_INIT, _SPACE_APP_PY


class CloudStandaloneLoadTests(unittest.TestCase):
    """Load training/cloud package (Space / notebook vendor path).

    Soft-imports of model_manifest / dataset_zip / contracts / train_core live
    in ``cloud.fallbacks`` so vendored notebooks still work without the full
    monorepo on ``sys.path``.
    """

    def test_default_batch_is_16(self) -> None:
        self.assertEqual(DEFAULT_BATCH, 16)

    def test_importlib_load_cloud_package(self) -> None:
        """Package import exposes public API (replaces single-file cloud.py load)."""
        self.assertTrue(_CLOUD_INIT.is_file(), f"missing {_CLOUD_INIT}")
        from cs2_vision_access.training import cloud as mod

        self.assertTrue(callable(getattr(mod, "create_manifest", None)))
        self.assertTrue(callable(getattr(mod, "extract_dataset", None)))
        self.assertEqual(getattr(mod, "DEFAULT_BATCH", None), 16)
        # Soft-import surface must exist for vendoring.
        from cs2_vision_access.training.cloud import fallbacks as fb

        self.assertTrue(callable(getattr(fb, "write_model_manifest", None)))
        self.assertTrue(callable(getattr(fb, "sha256_file", None)))

    def test_create_manifest_via_package_cloud(self) -> None:
        self.assertTrue(_CLOUD_INIT.is_file(), f"missing {_CLOUD_INIT}")
        from cs2_vision_access.training import cloud as mod

        with tempfile.TemporaryDirectory() as tmp:
            onnx_path = Path(tmp) / "model.onnx"
            onnx_path.write_bytes(b"fake_model_bytes")
            data_dir = Path(tmp) / "data"
            data_dir.mkdir()
            manifest_path = mod.create_manifest(onnx_path, data_dir)
            self.assertTrue(Path(manifest_path).is_file())
            payload = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
            self.assertEqual(
                payload["classes"],
                {str(k): v for k, v in DEFAULT_CLASSES.items()},
            )

    def test_space_app_loads_sibling_cloud_helpers(self) -> None:
        """Monorepo: space/app.py should bind helpers from package or sibling cloud/."""
        self.assertTrue(_SPACE_APP_PY.is_file(), f"missing {_SPACE_APP_PY}")
        mod_name = "_cs2_test_space_app"
        # Avoid clobbering a real import of the package space module.
        sys.modules.pop(mod_name, None)
        try:
            spec = importlib.util.spec_from_file_location(mod_name, _SPACE_APP_PY)
            self.assertIsNotNone(spec)
            assert spec is not None and spec.loader is not None
            mod = importlib.util.module_from_spec(spec)
            sys.modules[mod_name] = mod
            try:
                spec.loader.exec_module(mod)
            except ImportError as exc:
                # Gradio may be missing in minimal CI; still allow soft failure note.
                if "gradio" in str(exc).lower():
                    self.skipTest(f"gradio not installed: {exc}")
                raise
            self.assertIsNotNone(
                getattr(mod, "_cloud_train", None),
                "space app did not load cloud train helper "
                "(package, sibling ../cloud/, or vendored cloud)",
            )
            self.assertIsNotNone(getattr(mod, "_cloud_create_manifest", None))
        finally:
            sys.modules.pop(mod_name, None)


class CloudHelpersTests(unittest.TestCase):
    def test_find_dataset_zip_prefers_cs2_name(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "other.zip").write_bytes(b"PK\x03\x04")
            preferred = root / "cs2_dataset.zip"
            preferred.write_bytes(b"PK\x03\x04")
            found = find_dataset_zip([root])
            self.assertEqual(found, preferred)

    def test_find_dataset_zip_none(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(find_dataset_zip([tmp]))

    def test_package_outputs_includes_fp16_when_present(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            onnx = root / "cs2-yolo11n-seg.onnx"
            onnx.write_bytes(b"onnx-fp32")
            fp16 = root / "cs2-yolo11n-seg-fp16.onnx"
            fp16.write_bytes(b"onnx-fp16")
            manifest = root / "cs2-yolo11n-seg.model.json"
            manifest.write_text("{}", encoding="utf-8")
            out_zip = root / "bundle.zip"
            result = package_outputs(onnx, manifest, out_zip)
            self.assertEqual(result, out_zip)
            with zipfile.ZipFile(out_zip, "r") as zf:
                names = set(zf.namelist())
            self.assertEqual(
                names,
                {
                    "cs2-yolo11n-seg.onnx",
                    "cs2-yolo11n-seg-fp16.onnx",
                    "cs2-yolo11n-seg.model.json",
                },
            )

    def test_package_outputs_extra_paths_and_dirs(self) -> None:
        """extra_paths land at zip root; extra_dirs use arcname_prefix/rel."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            onnx = root / "cs2-yolo11n-seg.onnx"
            onnx.write_bytes(b"onnx-fp32")
            manifest = root / "cs2-yolo11n-seg.model.json"
            manifest.write_text("{}", encoding="utf-8")

            notes = root / "notes.txt"
            notes.write_text("ship it", encoding="utf-8")

            progress = root / "progress"
            progress.mkdir()
            report = progress / "report.md"
            report.write_text("# Progress\n", encoding="utf-8", newline="\n")
            plots = progress / "plots"
            plots.mkdir()
            (plots / "results.png").write_bytes(b"png-bytes")

            missing_file = root / "no-such-file.txt"
            missing_dir = root / "no-such-dir"

            out_zip = root / "bundle.zip"
            result = package_outputs(
                onnx,
                manifest,
                out_zip,
                extra_paths=[notes, missing_file],
                extra_dirs=[(progress, "progress"), (missing_dir, "gone")],
            )
            self.assertEqual(result, out_zip)
            with zipfile.ZipFile(out_zip, "r") as zf:
                names = set(zf.namelist())
            self.assertEqual(
                names,
                {
                    "cs2-yolo11n-seg.onnx",
                    "cs2-yolo11n-seg.model.json",
                    "notes.txt",
                    "progress/report.md",
                    "progress/plots/results.png",
                },
            )
            with zipfile.ZipFile(out_zip, "r") as zf:
                # Normalize newlines (Windows may write CRLF to source files).
                body = zf.read("progress/report.md").decode("utf-8").replace("\r\n", "\n")
                self.assertEqual(body, "# Progress\n")

    def test_run_pipeline_calls_package_outputs(self) -> None:
        """run_pipeline must package ONNX+manifest after a successful train.

        Patches target ``cloud.pipeline`` names where ``run_pipeline`` looks them up
        after the package split (not the facade re-exports on ``cloud``).
        """
        from cs2_vision_access.training import cloud as cloud_mod

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data_dir = root / "cs2_data"
            data_dir.mkdir()
            onnx = data_dir / "cs2-yolo11n-seg.onnx"
            onnx.write_bytes(b"onnx")
            manifest = data_dir / "cs2-yolo11n-seg.model.json"
            manifest.write_text("{}", encoding="utf-8")
            zip_path = root / "data.zip"
            zip_path.write_bytes(b"PK\x03\x04")

            pipeline = "cs2_vision_access.training.cloud.pipeline"
            with (
                patch(f"{pipeline}.install_dependencies") as mock_install,
                patch(f"{pipeline}.extract_dataset", return_value=data_dir) as mock_extract,
                patch(f"{pipeline}.train", return_value=onnx) as mock_train,
                patch(f"{pipeline}.create_manifest", return_value=manifest) as mock_manifest,
                patch(
                    f"{pipeline}.package_outputs",
                    return_value=data_dir / "cs2-yolo11n-seg-bundle.zip",
                ) as mock_pkg,
            ):
                result = cloud_mod.run_pipeline(
                    zip_path,
                    output_dir=str(data_dir),
                    resume=True,
                )
                mock_install.assert_called_once()
                mock_extract.assert_called_once()
                mock_train.assert_called_once()
                self.assertTrue(mock_train.call_args.kwargs["resume"])
                mock_manifest.assert_called_once()
                mock_pkg.assert_called_once()
                pkg_args = mock_pkg.call_args.args
                self.assertEqual(Path(pkg_args[0]), onnx)
                self.assertEqual(Path(pkg_args[1]), manifest)
                self.assertEqual(
                    Path(pkg_args[2]),
                    data_dir / "cs2-yolo11n-seg-bundle.zip",
                )
                self.assertEqual(result, (onnx, manifest))


class CloudOomRetryTests(unittest.TestCase):
    """Pure unit tests for cloud train OOM batch halving."""

    def test_retry_batch_on_oom_halves(self) -> None:
        self.assertEqual(_retry_batch_on_oom(16, RuntimeError("CUDA out of memory")), 8)
        self.assertEqual(_retry_batch_on_oom(8, RuntimeError("Out of memory")), 4)
        self.assertEqual(_retry_batch_on_oom(3, RuntimeError("cuda out of memory")), 1)
        self.assertEqual(_retry_batch_on_oom(1, RuntimeError("CUDA out of memory")), 1)

    def test_retry_batch_on_oom_non_oom_returns_none(self) -> None:
        self.assertIsNone(_retry_batch_on_oom(16, RuntimeError("invalid labels")))
        self.assertIsNone(_retry_batch_on_oom(8, ValueError("bad config")))

    def test_train_retries_on_cuda_oom_with_smaller_batch(self) -> None:
        """YOLO.train fails once with CUDA OOM, then succeeds at half batch."""
        from types import ModuleType, SimpleNamespace
        from unittest.mock import MagicMock

        from cs2_vision_access.training import cloud as cloud_mod

        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp)
            images = data_dir / "images"
            labels = data_dir / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "frame.jpg").write_bytes(b"img")
            (labels / "frame.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")

            run_dir = data_dir / "runs" / "train"
            weights = run_dir / "weights"
            weights.mkdir(parents=True)
            best_pt = weights / "best.pt"
            best_pt.write_bytes(b"pt")
            exported_onnx = best_pt.with_suffix(".onnx")
            exported_onnx.write_bytes(b"onnx")

            call_batches: list[int] = []

            def train_side_effect(**kwargs: object) -> SimpleNamespace:
                batch = int(kwargs["batch"])  # type: ignore[arg-type]
                call_batches.append(batch)
                if len(call_batches) == 1:
                    raise RuntimeError("CUDA out of memory. Tried to allocate 2.00 GiB")
                return SimpleNamespace(save_dir=str(run_dir))

            mock_model = MagicMock()
            mock_model.train.side_effect = train_side_effect
            mock_model.export.return_value = None
            mock_model.add_callback = MagicMock()

            fake_ultra = ModuleType("ultralytics")
            fake_ultra.YOLO = MagicMock(return_value=mock_model)  # type: ignore[attr-defined]

            with patch.dict(sys.modules, {"ultralytics": fake_ultra}):
                result = cloud_mod.train(
                    data_dir,
                    base_model="yolo11n-seg.pt",
                    epochs=1,
                    batch=16,
                    imgsz=416,
                    device="cpu",
                    plots=False,
                )

            self.assertEqual(call_batches, [16, 8])
            self.assertEqual(result, data_dir / cloud_mod.OUTPUT_ONNX_NAME)
            self.assertTrue(result.is_file())


class CloudInstallDependenciesTests(unittest.TestCase):
    def test_install_dependencies_skips_when_present(self) -> None:
        import subprocess

        with patch.object(subprocess, "run") as mock_run:
            # Pretend required modules import successfully.
            import sys
            from types import ModuleType

            fake_ultra = ModuleType("ultralytics")
            fake_onnx = ModuleType("onnx")
            fake_occ = ModuleType("onnxconverter_common")
            fake_torch = ModuleType("torch")
            fake_torch.cuda = type("cuda", (), {"is_available": staticmethod(lambda: False)})()

            with patch.dict(
                sys.modules,
                {
                    "ultralytics": fake_ultra,
                    "onnx": fake_onnx,
                    "onnxconverter_common": fake_occ,
                    "torch": fake_torch,
                },
            ):
                from cs2_vision_access.training.cloud import install_dependencies

                install_dependencies(gpu=False)
                mock_run.assert_not_called()

    def test_dependency_install_uses_trusted_shell_free_allowlisted_command(self) -> None:
        from cs2_vision_access.training.cloud import deps

        with patch.object(deps, "run") as mock_run:
            deps._install_allowed_packages(["onnx", "ultralytics"])

        command = mock_run.call_args.args[0]
        self.assertTrue(Path(command[0]).is_absolute())
        self.assertEqual(command[1:5], ["-m", "pip", "install", "-q"])
        self.assertEqual(command[5:], ["onnx", "ultralytics"])
        self.assertEqual(mock_run.call_args.kwargs["shell"], False)
        self.assertGreater(mock_run.call_args.kwargs["timeout"], 0)

    def test_dependency_install_rejects_non_allowlisted_package(self) -> None:
        from cs2_vision_access.training.cloud import deps

        with patch.object(deps, "run") as mock_run:
            with self.assertRaisesRegex(RuntimeError, "unapproved packages"):
                deps._install_allowed_packages(["onnx", "--requirement=/tmp/unsafe.txt"])
        mock_run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
