"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.training.auto import (
    EXIT_OK,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
)


class SelfTrainStageTests(unittest.TestCase):
    def test_self_train_disabled_no_ops(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_off",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {"enabled": False},
                }
            )
            with patch("cs2_vision_access.training.self_train.run_self_train_iteration") as mock_st:
                result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            mock_st.assert_not_called()
            self.assertEqual(result.state.artifacts.get("self_train_status"), "disabled")
            self.assertIn("self_train", result.state.completed_stages)

    def test_self_train_enabled_writes_artifact(self) -> None:
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_on",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {
                        "enabled": True,
                        "conf_threshold": 0.6,
                        "write_policy": "overwrite_pseudo",
                        "max_frames": 10,
                    },
                }
            )

            def fake_self_train(*args, **kwargs):
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=3,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.6,
                    labels_written=("a.txt", "b.txt", "c.txt"),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ) as mock_st:
                result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            mock_st.assert_called_once()
            call_kwargs = mock_st.call_args.kwargs
            self.assertEqual(call_kwargs["conf_threshold"], 0.6)
            self.assertAlmostEqual(call_kwargs["conf_low"], 0.6 * 0.7)
            self.assertEqual(call_kwargs["conf_high"], 0.6)
            self.assertEqual(call_kwargs["write_policy"], "overwrite_pseudo")
            self.assertEqual(call_kwargs["max_frames"], 10)
            self.assertEqual(result.state.artifacts.get("self_train_status"), "ok")
            self.assertEqual(result.state.artifacts.get("self_train_labels_written"), 3)
            report = result.state.artifacts.get("self_train_report")
            self.assertIsNotNone(report)
            self.assertTrue(Path(str(report)).is_file())
            self.assertIn("self_train", result.state.completed_stages)

    def test_self_train_config_parsed(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "flat_cloud",
                "run_id": "x",
                "sources": {"dataset_zip": "data/x.zip"},
                "train": {"backend": "cloud"},
                "self_train": {
                    "enabled": True,
                    "required": True,
                    "conf_threshold": 0.55,
                    "write_policy": "if_absent",
                    "max_frames": 5,
                    "conf_low": 0.3,
                    "conf_high": 0.7,
                    "write_uncertain_queue": False,
                    "retrain": True,
                    "retrain_required": True,
                },
            }
        )
        self.assertTrue(cfg.self_train.enabled)
        self.assertTrue(cfg.self_train.required)
        self.assertEqual(cfg.self_train.conf_threshold, 0.55)
        self.assertEqual(cfg.self_train.write_policy, "if_absent")
        self.assertEqual(cfg.self_train.max_frames, 5)
        self.assertEqual(cfg.self_train.conf_low, 0.3)
        self.assertEqual(cfg.self_train.conf_high, 0.7)
        self.assertFalse(cfg.self_train.write_uncertain_queue)
        self.assertTrue(cfg.self_train.retrain)
        self.assertTrue(cfg.self_train.retrain_required)

        # Teacher fields default: use_teacher on, no paths
        self.assertTrue(cfg.self_train.use_teacher)
        self.assertAlmostEqual(cfg.self_train.teacher_min_iou, 0.3)
        self.assertIsNone(cfg.self_train.teacher_model)
        self.assertIsNone(cfg.self_train.teacher_manifest)

    def test_self_train_config_teacher_fields(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "flat_cloud",
                "run_id": "x",
                "sources": {"dataset_zip": "data/x.zip"},
                "train": {"backend": "cloud"},
                "self_train": {
                    "enabled": True,
                    "use_teacher": True,
                    "teacher_min_iou": 0.4,
                    "teacher_model": "models/teacher.onnx",
                    "teacher_manifest": "models/teacher.model.json",
                },
            }
        )
        self.assertTrue(cfg.self_train.use_teacher)
        self.assertAlmostEqual(cfg.self_train.teacher_min_iou, 0.4)
        self.assertEqual(cfg.self_train.teacher_model, Path("models/teacher.onnx"))
        self.assertEqual(cfg.self_train.teacher_manifest, Path("models/teacher.model.json"))
        d = cfg.to_dict()["self_train"]
        self.assertTrue(d["use_teacher"])
        self.assertAlmostEqual(d["teacher_min_iou"], 0.4)
        self.assertEqual(Path(d["teacher_model"]), Path("models/teacher.onnx"))
        self.assertEqual(Path(d["teacher_manifest"]), Path("models/teacher.model.json"))

    def test_self_train_config_teacher_paths_must_pair(self) -> None:
        from cs2_vision_access.training.auto.config import AutoTrainConfigError

        with self.assertRaises(AutoTrainConfigError):
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "x",
                    "sources": {"dataset_zip": "data/x.zip"},
                    "train": {"backend": "cloud"},
                    "self_train": {
                        "enabled": True,
                        "teacher_model": "models/teacher.onnx",
                    },
                }
            )

    def test_self_train_passes_explicit_teacher(self) -> None:
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            teacher_onnx = root / "teacher.onnx"
            teacher_manifest = root / "teacher.model.json"
            teacher_onnx.write_bytes(b"onnx")
            teacher_manifest.write_text("{}", encoding="utf-8")
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_teacher",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {
                        "enabled": True,
                        "use_teacher": True,
                        "teacher_min_iou": 0.42,
                        "teacher_model": str(teacher_onnx),
                        "teacher_manifest": str(teacher_manifest),
                    },
                }
            )

            def fake_self_train(*args, **kwargs):
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=1,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.5,
                    labels_written=("a.txt",),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ) as mock_st:
                result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            mock_st.assert_called_once()
            call_kwargs = mock_st.call_args.kwargs
            self.assertEqual(Path(call_kwargs["teacher_model"]), teacher_onnx)
            self.assertEqual(Path(call_kwargs["teacher_manifest"]), teacher_manifest)
            self.assertAlmostEqual(float(call_kwargs["teacher_min_iou"]), 0.42)

    def test_self_train_retrain_calls_train_twice(self) -> None:
        """enabled + retrain + accepted>0 re-runs train+export."""
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_retrain",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {
                        "enabled": True,
                        "retrain": True,
                        "conf_threshold": 0.6,
                    },
                }
            )
            call_count = {"n": 0}

            def counting_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                call_count["n"] += 1
                return _mock_cloud_train(data_dir, **kwargs)

            def fake_self_train(*args, **kwargs):
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=2,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.6,
                    labels_written=("a.txt", "b.txt"),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=counting_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(call_count["n"], 2, "expected train then retrain")
            arts = result.state.artifacts
            self.assertEqual(arts.get("self_train_status"), "ok")
            self.assertEqual(arts.get("self_train_labels_written"), 2)
            self.assertEqual(arts.get("retrain_status"), "ok")
            self.assertIsNotNone(arts.get("retrain_onnx"))
            self.assertIsNotNone(arts.get("onnx_model"))
            self.assertEqual(arts.get("retrain_onnx"), arts.get("onnx_model"))

    def test_self_train_retrain_false_no_second_train(self) -> None:
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_no_retrain",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {
                        "enabled": True,
                        "retrain": False,
                    },
                }
            )
            call_count = {"n": 0}

            def counting_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                call_count["n"] += 1
                return _mock_cloud_train(data_dir, **kwargs)

            def fake_self_train(*args, **kwargs):
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=2,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.5,
                    labels_written=("a.txt", "b.txt"),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=counting_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(call_count["n"], 1)
            self.assertEqual(result.state.artifacts.get("retrain_status"), "disabled")

    def test_self_train_retrain_fail_soft_when_not_required(self) -> None:
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_retrain_soft",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"smoke_enabled": False},
                    "eval": {"enabled": False},
                    "self_train": {
                        "enabled": True,
                        "retrain": True,
                        "retrain_required": False,
                    },
                }
            )
            call_count = {"n": 0}

            def flaky_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                call_count["n"] += 1
                if call_count["n"] >= 2:
                    raise RuntimeError("boom retrain")
                return _mock_cloud_train(data_dir, **kwargs)

            def fake_self_train(*args, **kwargs):
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=1,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.5,
                    labels_written=("a.txt",),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=flaky_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(call_count["n"], 2)
            arts = result.state.artifacts
            self.assertEqual(arts.get("self_train_status"), "ok")
            self.assertEqual(arts.get("retrain_status"), "failed")
            self.assertIn("boom retrain", str(arts.get("retrain_error", "")))
            soft = arts.get("soft_notes") or []
            self.assertTrue(
                any("retrain failed" in str(n) for n in soft),
                f"expected soft note about retrain failure, got {soft!r}",
            )


if __name__ == "__main__":
    unittest.main()
