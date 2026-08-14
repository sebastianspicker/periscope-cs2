"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.training.auto import (
    EXIT_OK,
    STAGE_ORDER,
    AutoTrainConfigError,
    load_config,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from cs2_vision_access.training.auto.paths import build_run_paths
from cs2_vision_access.training.contracts import PRODUCT_CLASSES
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
)


class ConfigLoadTests(unittest.TestCase):
    def test_load_json_example_shape(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cfg.json"
            path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "mode": "flat_cloud",
                        "run_id": "demo",
                        "paths": {"work_root": str(Path(tmp) / "auto")},
                        "sources": {"dataset_zip": "data/x.zip"},
                        "train": {"backend": "cloud", "allow_leaky_val": True},
                        "export": {"promote_to_run_models": True},
                        "resume": {"enabled": True},
                    }
                ),
                encoding="utf-8",
            )
            cfg = load_config(path)
            self.assertEqual(cfg.mode, "flat_cloud")
            self.assertEqual(cfg.run_id, "demo")
            self.assertTrue(cfg.train.allow_leaky_val)
            self.assertEqual(cfg.train.class_names, PRODUCT_CLASSES)
            self.assertTrue(cfg.label.enabled)
            self.assertTrue(cfg.eval.enabled)
            self.assertFalse(cfg.self_train.enabled)
            self.assertTrue(cfg.train.resume_ultralytics)
            self.assertIsNone(cfg.train.min_map50)
            self.assertIsNone(cfg.train.base_model)
            self.assertTrue(cfg.export.smoke_enabled)
            self.assertFalse(cfg.export.smoke_required)

    def test_load_yaml(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cfg.yaml"
            path.write_text(
                "schema_version: 1\n"
                "mode: session_split\n"
                "run_id: y1\n"
                "sources:\n  prebuilt_dataset_root: data/session\n"
                "train:\n  backend: local\n",
                encoding="utf-8",
            )
            cfg = load_config(path)
            self.assertEqual(cfg.mode, "session_split")
            self.assertEqual(cfg.train.backend, "local")
            self.assertIsNone(cfg.train.base_model)

    def test_invalid_mode_fails(self) -> None:
        with self.assertRaises(AutoTrainConfigError):
            config_from_mapping({"schema_version": 1, "mode": "nope", "run_id": "x"})

    def test_invalid_profile_fails(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "x",
                    "train": {"profile": "not_a_real_profile"},
                }
            )
        self.assertIn("profile", str(ctx.exception).lower())

    def test_stage_order_includes_label_and_eval(self) -> None:
        self.assertEqual(
            list(STAGE_ORDER),
            [
                "ingest",
                "prepare_data",
                "label",
                "validate",
                "train",
                "export",
                "self_train",
                "eval",
                "report",
            ],
        )


class ConfigValidationTests(unittest.TestCase):
    def test_session_split_accepts_cloud_backend(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "session_split",
                "run_id": "x",
                "sources": {"prebuilt_dataset_root": "data/x"},
                "train": {"backend": "cloud"},
            }
        )
        self.assertEqual(cfg.mode, "session_split")
        self.assertEqual(cfg.train.backend, "cloud")

    def test_flat_cloud_requires_source(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "x",
                    "train": {"backend": "cloud"},
                }
            )
        self.assertIn("dataset_zip", str(ctx.exception).lower())

    def test_session_split_requires_source(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "x",
                    "train": {"backend": "local"},
                }
            )
        self.assertIn("prebuilt_dataset_root", str(ctx.exception).lower())

    def test_epochs_type_check(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "x",
                    "sources": {"dataset_zip": "data/x.zip"},
                    "train": {"backend": "cloud", "epochs": "many"},
                }
            )
        self.assertIn("epochs", str(ctx.exception).lower())

    def test_explicit_base_model_parsed(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "flat_cloud",
                "run_id": "x",
                "sources": {"dataset_zip": "data/x.zip"},
                "train": {"backend": "cloud", "base_model": "yolo11s-seg.pt"},
            }
        )
        self.assertEqual(cfg.train.base_model, "yolo11s-seg.pt")


class ProfileBaseModelTests(unittest.TestCase):
    def test_profile_base_model_used_when_config_omits(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "prof_base",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "profile": "cloud_t4",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                        "smoke": True,
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                }
            )
            seen: dict[str, object] = {}

            def capturing_train(data_dir: Path, **kwargs: object) -> Path:
                seen.update(kwargs)
                return _mock_cloud_train(data_dir, **kwargs)

            result = run_auto_train(cfg, train_cloud_fn=capturing_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            # smoke=True selects smoke profile → yolo26n-seg.pt
            self.assertEqual(seen.get("base_model"), "yolo26n-seg.pt")


class PathsTests(unittest.TestCase):
    def test_run_paths_layout(self) -> None:
        paths = build_run_paths("artifacts/auto", "demo")
        self.assertEqual(paths.run_dir, Path("artifacts/auto/demo"))
        self.assertEqual(paths.state_path, Path("artifacts/auto/demo/state.json"))
        self.assertEqual(paths.models_dir, Path("artifacts/auto/demo/models"))
        with self.assertRaises(ValueError):
            build_run_paths("artifacts/auto", "../evil")


if __name__ == "__main__":
    unittest.main()
