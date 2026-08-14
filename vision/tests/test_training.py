from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from cs2_vision_access.model_manifest import ModelManifest
from cs2_vision_access.training import (
    DEFAULT_BATCH,
    DEFAULT_EPOCHS,
    DEFAULT_IMAGE_SIZE,
    DEFAULT_PROJECT_DIRECTORY,
    DEFAULT_RUN_NAME,
    SMOKE_BATCH,
    SMOKE_EPOCHS,
    TrainingError,
    _normalise_class_names,
    _normalise_dataset_yaml_contract,
    resolve_train_hyperparameters,
    train_and_export,
)
from cs2_vision_access.training.local import _resolve_resume, _ultralytics_train_kwargs
from cs2_vision_access.training.train_core import build_train_kwargs


class TrainingContractTests(unittest.TestCase):
    def test_default_artifact_conventions(self) -> None:
        self.assertEqual(DEFAULT_PROJECT_DIRECTORY, Path("artifacts/runs/segment"))
        self.assertEqual(DEFAULT_RUN_NAME, "cs2-player")
        # Use as_posix() so the check is stable on Windows (str(Path) uses '\\').
        self.assertTrue(DEFAULT_PROJECT_DIRECTORY.as_posix().startswith("artifacts/"))

    def test_resolve_train_hyperparameters_full_defaults(self) -> None:
        epochs, batch, image_size = resolve_train_hyperparameters(
            smoke=False, epochs=None, batch=None, image_size=None
        )
        self.assertEqual(
            (epochs, batch, image_size), (DEFAULT_EPOCHS, DEFAULT_BATCH, DEFAULT_IMAGE_SIZE)
        )

    def test_resolve_train_hyperparameters_smoke_defaults(self) -> None:
        epochs, batch, image_size = resolve_train_hyperparameters(
            smoke=True, epochs=None, batch=None, image_size=None
        )
        self.assertEqual(
            (epochs, batch, image_size), (SMOKE_EPOCHS, SMOKE_BATCH, DEFAULT_IMAGE_SIZE)
        )

    def test_resolve_train_hyperparameters_explicit_overrides_smoke(self) -> None:
        epochs, batch, image_size = resolve_train_hyperparameters(
            smoke=True, epochs=3, batch=2, image_size=512
        )
        self.assertEqual((epochs, batch, image_size), (3, 2, 512))

    def test_class_names_accept_mapping_and_list_forms(self) -> None:
        self.assertEqual(_normalise_class_names({0: "player"}), {0: "player"})
        self.assertEqual(
            _normalise_class_names(["player", "corpse"]),
            {0: "player", 1: "corpse"},
        )

    def test_class_ids_must_be_contiguous(self) -> None:
        with self.assertRaisesRegex(TrainingError, "contiguous"):
            _normalise_class_names({1: "player"})

    def test_class_names_must_be_unique(self) -> None:
        with self.assertRaisesRegex(TrainingError, "unique"):
            _normalise_class_names({0: "player", 1: "PLAYER"})

    def test_build_train_kwargs_defaults_exist_ok_true(self) -> None:
        kwargs = build_train_kwargs(
            data="data.yaml",
            epochs=1,
            imgsz=640,
            batch=1,
            device="cpu",
            project="runs",
            name="exp",
        )
        self.assertTrue(kwargs["exist_ok"])
        self.assertTrue(kwargs["plots"])
        self.assertEqual(kwargs["workers"], 2)
        self.assertNotIn("resume", kwargs)
        self.assertNotIn("lr0", kwargs)
        self.assertNotIn("patience", kwargs)

    def test_build_train_kwargs_optional_and_resume(self) -> None:
        kwargs = build_train_kwargs(
            data="data.yaml",
            epochs=2,
            imgsz=416,
            batch=4,
            device="cuda:0",
            project="runs",
            name="exp",
            resume=True,
            lr0=0.001,
            patience=10,
            workers=None,
            amp=True,
        )
        self.assertTrue(kwargs["resume"])
        self.assertEqual(kwargs["lr0"], 0.001)
        self.assertEqual(kwargs["patience"], 10)
        self.assertTrue(kwargs["amp"])
        self.assertNotIn("workers", kwargs)

    def test_ultralytics_train_kwargs_omits_workers_by_default(self) -> None:
        kwargs = _ultralytics_train_kwargs(
            data="data.yaml",
            epochs=1,
            imgsz=640,
            batch=1,
            device="cpu",
            project="runs",
            name="exp",
        )
        self.assertTrue(kwargs["exist_ok"])
        self.assertNotIn("workers", kwargs)

    def test_resolve_resume_prefers_last_pt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            project = Path(temporary_directory)
            weights = project / "fixture" / "weights"
            weights.mkdir(parents=True)
            (weights / "last.pt").write_bytes(b"ckpt")
            self.assertEqual(
                _resolve_resume(
                    resume=True,
                    project_directory=project,
                    run_name="fixture",
                    exist_ok=False,
                ),
                (True, True),
            )

    def test_resolve_resume_missing_last_pt_trains_from_scratch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            project = Path(temporary_directory)
            self.assertEqual(
                _resolve_resume(
                    resume=True,
                    project_directory=project,
                    run_name="fixture",
                    exist_ok=False,
                ),
                (False, True),
            )

    def test_resolve_resume_false_keeps_exist_ok(self) -> None:
        self.assertEqual(
            _resolve_resume(
                resume=False,
                project_directory="runs",
                run_name="fixture",
                exist_ok=False,
            ),
            (False, False),
        )

    def test_backend_receives_absolute_validated_dataset_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            dataset = root / "dataset"
            for split in ("train", "val"):
                (dataset / "images" / split).mkdir(parents=True)
                (dataset / "labels" / split).mkdir(parents=True)
                (dataset / "images" / split / f"{split}.png").write_bytes(b"image")
                (dataset / "labels" / split / f"{split}.txt").write_text(
                    "",
                    encoding="utf-8",
                )
            config = root / "dataset.yaml"
            config.write_text(
                json.dumps(
                    {
                        "path": str(dataset),
                        "train": "images/train",
                        "val": "images/val",
                        "names": {"0": "player"},
                        "download": "must not reach the backend",
                    }
                ),
                encoding="utf-8",
            )
            base_model = root / "trusted.pt"
            base_model.write_bytes(b"trusted local fixture")
            run_directory = root / "runs" / "fixture"
            received_configs: list[tuple[str, dict[str, object]]] = []
            received_train_kwargs: list[dict[str, object]] = []
            export_calls: list[dict[str, object]] = []

            class FakeYolo:
                def __init__(self, source: str, *, task: str) -> None:
                    self.source = source
                    self.task = task

                def train(self, **arguments: object) -> object:
                    data_path = Path(str(arguments["data"]))
                    received_configs.append(
                        (
                            str(data_path),
                            json.loads(data_path.read_text(encoding="utf-8")),
                        )
                    )
                    received_train_kwargs.append(dict(arguments))
                    (run_directory / "weights").mkdir(parents=True)
                    (run_directory / "weights" / "best.pt").write_bytes(b"trained")
                    return SimpleNamespace(save_dir=str(run_directory))

                def export(self, **arguments: object) -> str:
                    export_calls.append(arguments)
                    exported = Path(self.source).with_suffix(".onnx")
                    exported.write_bytes(b"onnx fixture")
                    return str(exported)

            fake_yaml = SimpleNamespace(
                safe_load=json.loads,
                YAMLError=ValueError,
            )
            fake_ultralytics = SimpleNamespace(YOLO=FakeYolo)
            call_arguments = {
                "dataset_yaml": config,
                "dataset_root": dataset,
                "class_names": {0: "player"},
                "base_model": base_model,
                "base_model_origin": "local test fixture",
                "exported_model_license": "test-only",
                "allow_model_download": False,
                "epochs": 1,
                "image_size": 640,
                "batch": 1,
                "device": "cpu",
                "project_directory": root / "runs",
                "run_name": "fixture",
            }
            with (
                patch.dict(
                    sys.modules,
                    {
                        "onnx": None,
                        "ultralytics": fake_ultralytics,
                        "yaml": fake_yaml,
                    },
                ),
                self.assertRaisesRegex(TrainingError, "training extra"),
            ):
                train_and_export(**call_arguments)

            self.assertEqual(received_configs, [])
            with patch.dict(
                sys.modules,
                {
                    "onnx": SimpleNamespace(),
                    "ultralytics": fake_ultralytics,
                    "yaml": fake_yaml,
                },
            ):
                summary = train_and_export(**call_arguments)

            self.assertEqual(len(received_configs), 1)
            received_path, received = received_configs[0]
            self.assertNotEqual(received_path, str(config))
            self.assertEqual(received["path"], str(dataset.resolve()))
            self.assertEqual(received["names"], ["player"])
            self.assertNotIn("download", received)
            self.assertEqual(export_calls[0]["simplify"], False)
            train_kwargs = received_train_kwargs[0]
            self.assertTrue(train_kwargs["exist_ok"])
            self.assertTrue(train_kwargs["plots"])
            self.assertNotIn("resume", train_kwargs)
            self.assertNotIn("lr0", train_kwargs)
            self.assertNotIn("workers", train_kwargs)
            manifest = ModelManifest.load(summary.manifest)
            self.assertEqual(manifest.origin, "local-training; base=local test fixture")
            self.assertEqual(manifest.license, "test-only")

    def test_resume_true_with_last_pt_passes_resume(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            dataset = root / "dataset"
            for split in ("train", "val"):
                (dataset / "images" / split).mkdir(parents=True)
                (dataset / "labels" / split).mkdir(parents=True)
                (dataset / "images" / split / f"{split}.png").write_bytes(b"image")
                (dataset / "labels" / split / f"{split}.txt").write_text(
                    "",
                    encoding="utf-8",
                )
            config = root / "dataset.yaml"
            config.write_text(
                json.dumps(
                    {
                        "path": str(dataset),
                        "train": "images/train",
                        "val": "images/val",
                        "names": {"0": "player"},
                    }
                ),
                encoding="utf-8",
            )
            base_model = root / "trusted.pt"
            base_model.write_bytes(b"trusted")
            run_directory = root / "runs" / "fixture"
            (run_directory / "weights").mkdir(parents=True)
            (run_directory / "weights" / "last.pt").write_bytes(b"last")
            train_kwargs_seen: list[dict[str, object]] = []

            class FakeYolo:
                def __init__(self, source: str, *, task: str) -> None:
                    self.source = source

                def train(self, **arguments: object) -> object:
                    train_kwargs_seen.append(dict(arguments))
                    (run_directory / "weights" / "best.pt").write_bytes(b"trained")
                    return SimpleNamespace(save_dir=str(run_directory))

                def export(self, **arguments: object) -> str:
                    exported = Path(self.source).with_suffix(".onnx")
                    exported.write_bytes(b"onnx")
                    return str(exported)

            fake_yaml = SimpleNamespace(safe_load=json.loads, YAMLError=ValueError)
            with patch.dict(
                sys.modules,
                {
                    "onnx": SimpleNamespace(),
                    "ultralytics": SimpleNamespace(YOLO=FakeYolo),
                    "yaml": fake_yaml,
                },
            ):
                train_and_export(
                    dataset_yaml=config,
                    dataset_root=dataset,
                    class_names={0: "player"},
                    base_model=base_model,
                    base_model_origin="local test fixture",
                    exported_model_license="test-only",
                    allow_model_download=False,
                    epochs=1,
                    image_size=640,
                    batch=1,
                    device="cpu",
                    project_directory=root / "runs",
                    run_name="fixture",
                    resume=True,
                    lr0=0.002,
                    patience=5,
                    workers=0,
                )

            self.assertEqual(len(train_kwargs_seen), 1)
            kwargs = train_kwargs_seen[0]
            self.assertTrue(kwargs["resume"])
            self.assertTrue(kwargs["exist_ok"])
            self.assertEqual(kwargs["lr0"], 0.002)
            self.assertEqual(kwargs["patience"], 5)
            self.assertEqual(kwargs["workers"], 0)

    def test_configured_test_split_must_exist(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            dataset = root / "dataset"
            dataset.mkdir()
            config = root / "dataset.yaml"
            config.write_text(
                json.dumps(
                    {
                        "path": str(dataset),
                        "train": "images/train",
                        "val": "images/val",
                        "test": "images/test",
                        "names": ["player"],
                    }
                ),
                encoding="utf-8",
            )
            fake_yaml = SimpleNamespace(safe_load=json.loads, YAMLError=ValueError)

            with patch.dict(sys.modules, {"yaml": fake_yaml}):
                with self.assertRaisesRegex(TrainingError, "test split"):
                    _normalise_dataset_yaml_contract(
                        config,
                        dataset,
                        {0: "player"},
                    )


if __name__ == "__main__":
    unittest.main()
