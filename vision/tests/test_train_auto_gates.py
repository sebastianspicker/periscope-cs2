"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock

from cs2_vision_access.training.auto import (
    EXIT_HUMAN_GATE,
    EXIT_OK,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from cs2_vision_access.training.auto.paths import build_run_paths
from cs2_vision_access.training.auto.state import StageState
from cs2_vision_access.training.local import TrainingSummary
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
    _write_split_dataset,
)


class HumanGateTests(unittest.TestCase):
    def test_human_gate_blocks_before_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "gate1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "human_gate": {
                        "enabled": True,
                        "block": True,
                        "message": "review required",
                    },
                }
            )
            train_mock = MagicMock(side_effect=AssertionError("train must not run"))
            result = run_auto_train(cfg, train_cloud_fn=train_mock)
            self.assertEqual(result.exit_code, EXIT_HUMAN_GATE)
            self.assertEqual(result.state.status, "human_gate")
            train_mock.assert_not_called()
            self.assertIn("validate", result.state.completed_stages)
            self.assertIn("label", result.state.completed_stages)
            self.assertNotIn("train", result.state.completed_stages)
            self.assertIn("review required", result.message or "")
            self.assertIn("human_gate_report:", result.message or "")
            gate_report = work / "gate1" / "progress" / "human_gate.json"
            self.assertTrue(gate_report.is_file(), "human_gate.json must be written")
            payload = json.loads(gate_report.read_text(encoding="utf-8"))
            self.assertTrue(payload["blocked"])
            self.assertEqual(payload["message"].split(" | ")[0], "review required")
            self.assertIn("validate", payload["completed_stages"])
            self.assertIn("label", payload["completed_stages"])
            self.assertNotIn("train", payload["completed_stages"])

    def test_human_gate_enriches_message_with_uncertain_review(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            run_dir = work / "gate_review"
            progress = run_dir / "progress"
            progress.mkdir(parents=True)
            review_path = progress / "uncertain_review.json"
            review_path.write_text(
                json.dumps({"schema_version": 1, "items": [{"stem": "a", "score": 0.9}]}),
                encoding="utf-8",
            )
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "gate_review",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "human_gate": {
                        "enabled": True,
                        "block": True,
                        "message": "operator review",
                    },
                }
            )
            result = run_auto_train(
                cfg, train_cloud_fn=MagicMock(side_effect=AssertionError("no train"))
            )
            self.assertEqual(result.exit_code, EXIT_HUMAN_GATE)
            msg = result.message or ""
            self.assertIn("uncertain_review:", msg)
            self.assertIn("uncertain_review.json", msg)
            self.assertIn("human_gate_report:", msg)
            gate_report = progress / "human_gate.json"
            self.assertTrue(gate_report.is_file())
            payload = json.loads(gate_report.read_text(encoding="utf-8"))
            self.assertIsNotNone(payload["uncertain_review"])
            self.assertTrue(str(payload["uncertain_review"]).endswith("uncertain_review.json"))

    def test_stage_human_gate_direct_with_report(self) -> None:
        """Drive shipped stage_human_gate entry without full pipeline."""
        from cs2_vision_access.training.auto.errors import HumanGateBlocked
        from cs2_vision_access.training.auto.stages import stage_human_gate

        with tempfile.TemporaryDirectory() as tmp:
            paths = build_run_paths(str(Path(tmp)), "run")
            state = StageState(
                run_id="run",
                mode="flat_cloud",
                completed_stages=["ingest", "prepare_data", "label", "validate"],
                artifacts={"dataset_root": str(paths.run_dir / "data"), "label_count": 3},
            )
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "run",
                    "paths": {"work_root": str(tmp)},
                    "sources": {"prebuilt_flat_root": str(tmp)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "p"},
                    },
                    "human_gate": {
                        "enabled": True,
                        "block": True,
                        "message": "stop here",
                    },
                }
            )
            with self.assertRaises(HumanGateBlocked) as ctx:
                stage_human_gate(cfg, state, paths)
            self.assertIn("stop here", str(ctx.exception))
            self.assertIn("human_gate_report:", str(ctx.exception))
            report = paths.run_dir / "progress" / "human_gate.json"
            self.assertTrue(report.is_file())
            body = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(body["label_count"], 3)
            self.assertTrue(body["blocked"])


class MinMap50StrictTests(unittest.TestCase):
    def test_min_map50_fails_when_metrics_missing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "split"
            _write_split_dataset(data, class_names={0: "ct"})
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "map_missing",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "local",
                        "class_names": {"0": "ct"},
                        "smoke": True,
                        "min_map50": 0.5,
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                }
            )

            def local_train_no_csv(**kwargs: object) -> TrainingSummary:
                run_dir = Path(str(kwargs["project_directory"])) / str(kwargs["run_name"])
                run_dir.mkdir(parents=True, exist_ok=True)
                weights = run_dir / "weights"
                weights.mkdir(parents=True, exist_ok=True)
                onnx = weights / "best.onnx"
                manifest = weights / "best.model.json"
                onnx.write_bytes(b"fake-onnx")
                manifest.write_text(
                    json.dumps(
                        {
                            "schema_version": 1,
                            "model_filename": "best.onnx",
                            "sha256": "0" * 64,
                            "task": "instance-segmentation",
                            "classes": {"0": "ct"},
                            "origin": "test",
                            "license": "AGPL-3.0-only",
                        }
                    ),
                    encoding="utf-8",
                )
                return TrainingSummary(
                    run_directory=str(run_dir),
                    best_checkpoint=str(weights / "best.pt"),
                    onnx_model=str(onnx),
                    manifest=str(manifest),
                )

            with self.assertRaises(AutoTrainError) as ctx:
                run_auto_train(cfg, train_local_fn=local_train_no_csv)
            self.assertIn("map50", str(ctx.exception).lower())


class ExportSmokeFlagTests(unittest.TestCase):
    def test_smoke_disabled_skips_inference(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "nosmoke",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                }
            )
            result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(result.state.artifacts.get("smoke_inference"), "disabled")

    def test_smoke_required_fails_on_bad_onnx(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "smoke_req",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": True, "smoke_required": True},
                    "eval": {"enabled": False},
                }
            )
            with self.assertRaises(AutoTrainError) as ctx:
                run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertIn("smoke", str(ctx.exception).lower())


class HumanGateBeforeStartTests(unittest.TestCase):
    def test_human_gate_does_not_mark_train_started(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "gate_before",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "human_gate": {
                        "enabled": True,
                        "block": True,
                        "message": "review required",
                    },
                }
            )
            result = run_auto_train(cfg, train_cloud_fn=MagicMock(side_effect=AssertionError("no")))
            self.assertEqual(result.exit_code, EXIT_HUMAN_GATE)
            self.assertIsNone(result.state.current_stage)
            self.assertNotIn("train", result.state.completed_stages)


if __name__ == "__main__":
    unittest.main()
