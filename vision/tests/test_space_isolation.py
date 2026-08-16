"""Security boundaries for the Hugging Face training Space."""

from __future__ import annotations

import importlib.util
import shutil
import sys
import tempfile
import threading
import unittest
import zipfile
from pathlib import Path
from types import ModuleType
from unittest.mock import patch


class _Progress:
    def __call__(self, *args: object, **kwargs: object) -> None:
        return None


def _load_space_app() -> ModuleType:
    gradio = ModuleType("gradio")
    gradio.Progress = _Progress  # type: ignore[attr-defined]
    app_path = Path(__file__).resolve().parents[1] / "src/cs2_vision_access/training/space/app.py"
    spec = importlib.util.spec_from_file_location("space_app_isolation_test", app_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load Space app test module")
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {"gradio": gradio}):
        spec.loader.exec_module(module)
    return module


_SPACE_APP = _load_space_app()


class SpaceIsolationTests(unittest.TestCase):
    def _zip_path(self, root: Path, name: str = "upload.zip") -> Path:
        zip_path = root / name
        with zipfile.ZipFile(zip_path, "w") as archive:
            archive.writestr("images/frame.jpg", b"image")
            archive.writestr("labels/frame.txt", b"0 0.5 0.5 0.1 0.1\n")
        return zip_path

    def test_upload_member_limit_stops_before_extract(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            zip_path = self._zip_path(Path(tmp))
            with (
                patch.object(_SPACE_APP, "MAX_ZIP_MEMBERS", 1),
                patch.object(_SPACE_APP, "extract_dataset") as extract,
            ):
                result = _SPACE_APP._train(
                    zip_path,
                    "yolo11n-seg.pt",
                    150,
                    16,
                    416,
                    0.001,
                    50,
                    _Progress(),
                )
        self.assertIn("too many members", result[0])
        extract.assert_not_called()

    def test_hostile_path_cannot_supply_nvidia_smi(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            malicious = Path(tmp) / "nvidia-smi"
            malicious.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            malicious.chmod(0o700)
            environment = {
                "PATH": tmp,
                _SPACE_APP._TRUSTED_NVIDIA_SMI_ENV: "",
            }
            with (
                patch.dict(_SPACE_APP.os.environ, environment, clear=False),
                patch.object(_SPACE_APP, "_PLATFORM_NVIDIA_SMI_PATHS", ()),
                patch.object(_SPACE_APP.shutil, "which", side_effect=AssertionError("PATH used")),
            ):
                self.assertIsNone(_SPACE_APP._nvidia_smi_path())

    def test_explicit_trusted_nvidia_smi_path_is_validated(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            executable = Path(tmp) / "nvidia-smi"
            executable.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            executable.chmod(0o700)
            with patch.dict(
                _SPACE_APP.os.environ,
                {_SPACE_APP._TRUSTED_NVIDIA_SMI_ENV: str(executable)},
                clear=False,
            ):
                self.assertEqual(_SPACE_APP._nvidia_smi_path(), str(executable.resolve()))

    def test_concurrent_sessions_fail_closed_and_keep_artifacts_isolated(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = self._zip_path(root)
            started = threading.Event()
            release = threading.Event()
            results: list[tuple[str, str | None, str | None, str | None, str]] = []
            train_calls = 0

            def extract_dataset(_: object, output_dir: Path) -> Path:
                data_dir = Path(output_dir)
                (data_dir / "images").mkdir(parents=True)
                (data_dir / "labels").mkdir()
                (data_dir / "images" / "frame.jpg").write_bytes(b"image")
                (data_dir / "labels" / "frame.txt").write_text("0 0.5 0.5 0.1 0.1\n")
                (data_dir / "dataset.yaml").write_text("untrusted: true\n", encoding="utf-8")
                return data_dir

            def train_model(data_dir: Path, **_: object) -> Path:
                nonlocal train_calls
                train_calls += 1
                if train_calls == 1:
                    started.set()
                    self.assertTrue(release.wait(timeout=5))
                model = data_dir / "model.onnx"
                model.write_bytes(f"model-{train_calls}".encode())
                return model

            def create_manifest(_: Path, data_dir: Path, **__: object) -> Path:
                manifest = data_dir / "model.json"
                manifest.write_text("{}", encoding="utf-8")
                return manifest

            def run_first() -> None:
                results.append(
                    _SPACE_APP._train(
                        zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                    )
                )

            with (
                patch.object(_SPACE_APP, "_TRAINING_SLOT", threading.BoundedSemaphore(1)),
                patch.object(_SPACE_APP, "extract_dataset", side_effect=extract_dataset),
                patch.object(_SPACE_APP, "train_model", side_effect=train_model),
                patch.object(_SPACE_APP, "create_manifest", side_effect=create_manifest),
                patch.object(_SPACE_APP, "_has_cuda", return_value=False),
                patch.object(_SPACE_APP, "_gpu_info", return_value="None (CPU)"),
            ):
                first = threading.Thread(target=run_first)
                first.start()
                self.assertTrue(started.wait(timeout=5))
                blocked = _SPACE_APP._train(
                    zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )
                self.assertIn("already running", blocked[0])
                release.set()
                first.join(timeout=5)
                self.assertFalse(first.is_alive())
                second = _SPACE_APP._train(
                    zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertEqual(len(results), 1)
            first_onnx = Path(results[0][1] or "")
            second_onnx = Path(second[1] or "")
            self.assertNotEqual(first_onnx.parent, second_onnx.parent)
            self.assertNotEqual(first_onnx.name, second_onnx.name)
            self.assertEqual(first_onnx.read_bytes(), b"model-1")
            self.assertEqual(second_onnx.read_bytes(), b"model-2")
            shutil.rmtree(first_onnx.parent)
            shutil.rmtree(second_onnx.parent)

    def test_uploaded_path_is_replaced_before_training(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "upload.zip"
            with zipfile.ZipFile(zip_path, "w") as archive:
                archive.writestr("images/frame.jpg", b"image")
                archive.writestr("labels/frame.txt", "3 0.5 0.5 0.1 0.1\n")
                archive.writestr(
                    "dataset.yaml",
                    "path: /outside\ntrain: ../../outside\nval: nowhere\nnc: 999\n",
                )

            observed_manifests: list[tuple[Path, str]] = []

            def train_model(data_dir: Path, **_: object) -> Path:
                observed_manifests.append(
                    (data_dir, (data_dir / "dataset.yaml").read_text(encoding="utf-8"))
                )
                model = data_dir / "model.onnx"
                model.write_bytes(b"model")
                return model

            def create_manifest(_: Path, data_dir: Path, **__: object) -> Path:
                manifest = data_dir / "model.json"
                manifest.write_text("{}", encoding="utf-8")
                return manifest

            with (
                patch.object(_SPACE_APP, "train_model", side_effect=train_model),
                patch.object(_SPACE_APP, "create_manifest", side_effect=create_manifest),
                patch.object(_SPACE_APP, "_has_cuda", return_value=False),
                patch.object(_SPACE_APP, "_gpu_info", return_value="None (CPU)"),
            ):
                result = _SPACE_APP._train(
                    zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertIn("Training complete", result[0])
            self.assertEqual(len(observed_manifests), 1)
            data_dir, manifest = observed_manifests[0]
            self.assertIn(f"path: {data_dir.resolve().as_posix()}", manifest)
            self.assertIn("train: images\nval: images\nnc: 4", manifest)
            self.assertIn("  0: ct\n  1: ct_head\n  2: t\n  3: t_head", manifest)
            self.assertNotIn("/outside", manifest)
            self.assertNotIn("../../outside", manifest)
            shutil.rmtree(Path(result[1] or "").parent)

    def test_uploaded_download_never_executes_or_reaches_training(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            marker = root / "executed"
            zip_path = root / "upload.zip"
            with zipfile.ZipFile(zip_path, "w") as archive:
                archive.writestr("images/frame.jpg", b"image")
                archive.writestr("labels/frame.txt", "0 0.5 0.5 0.1 0.1\n")
                archive.writestr(
                    "dataset.yaml",
                    f"download: python -c \\\"Path({marker!r}).touch()\\\"\n",
                )

            observed_manifests: list[str] = []

            def train_model(data_dir: Path, **_: object) -> Path:
                observed_manifests.append((data_dir / "dataset.yaml").read_text(encoding="utf-8"))
                model = data_dir / "model.onnx"
                model.write_bytes(b"model")
                return model

            def create_manifest(_: Path, data_dir: Path, **__: object) -> Path:
                manifest = data_dir / "model.json"
                manifest.write_text("{}", encoding="utf-8")
                return manifest

            with (
                patch.object(_SPACE_APP, "train_model", side_effect=train_model),
                patch.object(_SPACE_APP, "create_manifest", side_effect=create_manifest),
                patch.object(_SPACE_APP, "_has_cuda", return_value=False),
                patch.object(_SPACE_APP, "_gpu_info", return_value="None (CPU)"),
            ):
                result = _SPACE_APP._train(
                    zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertIn("Training complete", result[0])
            self.assertEqual(len(observed_manifests), 1)
            self.assertNotIn("download", observed_manifests[0])
            self.assertFalse(marker.exists())
            shutil.rmtree(Path(result[1] or "").parent)

    def test_invalid_label_class_fails_before_training(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zip_path = root / "upload.zip"
            with zipfile.ZipFile(zip_path, "w") as archive:
                archive.writestr("images/frame.jpg", b"image")
                archive.writestr("labels/frame.txt", "4 0.5 0.5 0.1 0.1\n")

            with patch.object(_SPACE_APP, "train_model") as train_model:
                result = _SPACE_APP._train(
                    zip_path, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertIn("Error validating dataset metadata", result[0])
            train_model.assert_not_called()

    def test_failed_job_removes_extraction_and_artifact_directories(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            extract_dir = root / "extract"
            artifact_dir = root / "artifacts"
            upload = self._zip_path(root)
            with (
                patch.object(
                    _SPACE_APP.tempfile,
                    "mkdtemp",
                    side_effect=[str(extract_dir), str(artifact_dir)],
                ),
                patch.object(_SPACE_APP, "extract_dataset", side_effect=ValueError("bad archive")),
                patch.object(_SPACE_APP, "_has_cuda", return_value=False),
                patch.object(_SPACE_APP, "_gpu_info", return_value="None (CPU)"),
            ):
                result = _SPACE_APP._train_job(
                    upload, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertIn("Error extracting zip", result[0])
            self.assertFalse(extract_dir.exists())
            self.assertFalse(artifact_dir.exists())

    def test_extractor_cannot_return_dataset_outside_private_root(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            extract_dir = root / "extract"
            artifact_dir = root / "artifacts"
            outside = root / "outside"
            extract_dir.mkdir()
            artifact_dir.mkdir()
            (outside / "images").mkdir(parents=True)
            (outside / "labels").mkdir()
            upload = self._zip_path(root)
            with (
                patch.object(
                    _SPACE_APP.tempfile,
                    "mkdtemp",
                    side_effect=[str(extract_dir), str(artifact_dir)],
                ),
                patch.object(_SPACE_APP, "extract_dataset", return_value=outside),
                patch.object(_SPACE_APP, "train_model") as train_model,
                patch.object(_SPACE_APP, "_has_cuda", return_value=False),
                patch.object(_SPACE_APP, "_gpu_info", return_value="None (CPU)"),
            ):
                result = _SPACE_APP._train_job(
                    upload, "yolo11n-seg.pt", 150, 16, 416, 0.001, 50, _Progress()
                )

            self.assertIn("escaped the private extraction directory", result[0])
            train_model.assert_not_called()
            self.assertFalse(extract_dir.exists())
            self.assertFalse(artifact_dir.exists())

    def test_required_space_auth_rejects_missing_or_blank_values(self) -> None:
        invalid_environments = (
            {"CS2_SPACE_AUTH_PASSWORD": "secret"},
            {"CS2_SPACE_AUTH_USER": "user"},
            {"CS2_SPACE_AUTH_USER": " ", "CS2_SPACE_AUTH_PASSWORD": "secret"},
        )
        for environment in invalid_environments:
            with self.subTest(environment=environment):
                with patch.dict(_SPACE_APP.os.environ, environment, clear=True):
                    with self.assertRaisesRegex(RuntimeError, "CS2_SPACE_AUTH_USER"):
                        _SPACE_APP._required_space_auth()

    def test_main_launches_once_with_private_space_options(self) -> None:
        class _Demo:
            def __init__(self) -> None:
                self.launch_calls: list[dict[str, object]] = []

            def launch(self, **kwargs: object) -> None:
                self.launch_calls.append(kwargs)

        demo = _Demo()
        with (
            patch.object(_SPACE_APP, "_required_space_auth", return_value=("user", "pass")),
            patch.object(_SPACE_APP, "_build_ui", return_value=demo),
        ):
            _SPACE_APP.main()

        self.assertEqual(
            demo.launch_calls,
            [
                {
                    "auth": ("user", "pass"),
                    "max_file_size": _SPACE_APP.MAX_UPLOAD_BYTES,
                    "show_error": False,
                    "enable_monitoring": False,
                }
            ],
        )


if __name__ == "__main__":
    unittest.main()
