"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.training.auto import (
    EXIT_OK,
    AutoTrainConfigError,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
)


class SelfTrainStageMoreTests(unittest.TestCase):
    def test_self_train_retrain_required_hard_fails(self) -> None:
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
                    "run_id": "st_retrain_hard",
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
                        "retrain_required": True,
                    },
                }
            )
            call_count = {"n": 0}

            def flaky_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                call_count["n"] += 1
                if call_count["n"] >= 2:
                    raise RuntimeError("boom retrain hard")
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

            with (
                patch(
                    "cs2_vision_access.training.self_train.run_self_train_iteration",
                    side_effect=fake_self_train,
                ),
                self.assertRaises(AutoTrainError) as ctx,
            ):
                run_auto_train(cfg, train_cloud_fn=flaky_cloud_train)
            self.assertIn("retrain failed", str(ctx.exception).lower())

    def test_self_train_retrain_skipped_when_no_labels(self) -> None:
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
                    "run_id": "st_retrain_zero",
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
                    already_labeled=1,
                    unlabeled_scanned=0,
                    accepted=0,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=0.5,
                    labels_written=(),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=counting_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(call_count["n"], 1)
            self.assertEqual(result.state.artifacts.get("retrain_status"), "skipped_no_labels")

    def test_self_train_iterations_two_calls_train_three(self) -> None:
        """iterations=2 + accepted>0 → self_train×2, train = 1 initial + 2 retrain."""
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
                    "run_id": "st_iters2",
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
                        "iterations": 2,
                        "conf_threshold": 0.5,
                        "conf_schedule": True,
                        "stop_on_no_growth": True,
                        "max_plateau_iters": 2,
                    },
                }
            )
            self.assertTrue(cfg.self_train.retrain)
            train_count = {"n": 0}
            st_count = {"n": 0}

            def counting_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                train_count["n"] += 1
                return _mock_cloud_train(data_dir, **kwargs)

            def fake_self_train(*args, **kwargs):
                st_count["n"] += 1
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                conf = float(kwargs.get("conf_threshold", 0.5))
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=0,
                    unlabeled_scanned=1,
                    accepted=2,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=conf,
                    labels_written=("a.txt", "b.txt"),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=counting_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(st_count["n"], 2, "expected self_train twice")
            self.assertEqual(
                train_count["n"],
                3,
                "expected initial train + 2 retrain",
            )
            arts = result.state.artifacts
            self.assertEqual(arts.get("self_train_status"), "ok")
            self.assertEqual(arts.get("self_train_iterations_completed"), 2)
            self.assertEqual(arts.get("self_train_stopped_reason"), "completed")
            history = arts.get("self_train_history") or []
            self.assertEqual(len(history), 2)
            self.assertEqual(history[0]["iteration"], 1)
            self.assertEqual(history[1]["iteration"], 2)
            self.assertEqual(history[0]["accepted"], 2)
            self.assertEqual(history[1]["retrain_status"], "ok")

    def test_self_train_plateau_stops_without_infinite_retrain(self) -> None:
        """accepted=0 with stop_on_no_growth stops after max_plateau_iters."""
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
                    "run_id": "st_plateau",
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
                        "iterations": 5,
                        "stop_on_no_growth": True,
                        "max_plateau_iters": 2,
                    },
                }
            )
            train_count = {"n": 0}
            st_count = {"n": 0}

            def counting_cloud_train(data_dir: Path, **kwargs: object) -> Path:
                train_count["n"] += 1
                return _mock_cloud_train(data_dir, **kwargs)

            def fake_self_train(*args, **kwargs):
                st_count["n"] += 1
                report_path = Path(kwargs["report_path"])
                report_path.parent.mkdir(parents=True, exist_ok=True)
                report_path.write_text("{}", encoding="utf-8")
                return SelfTrainReport(
                    images_scanned=1,
                    already_labeled=1,
                    unlabeled_scanned=0,
                    accepted=0,
                    rejected_low_conf=0,
                    rejected_empty=0,
                    conf_threshold=float(kwargs.get("conf_threshold", 0.5)),
                    labels_written=(),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=counting_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(st_count["n"], 2, "plateau after 2 zero-accept cycles")
            self.assertEqual(train_count["n"], 1, "no retrain when accepted==0")
            arts = result.state.artifacts
            self.assertEqual(arts.get("self_train_stopped_reason"), "plateau")
            self.assertEqual(arts.get("self_train_iterations_completed"), 2)
            self.assertEqual(arts.get("retrain_status"), "skipped_no_labels")

    def test_autonomous_true_enables_self_train_iterations_three(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "flat_cloud",
                "run_id": "auto_root",
                "autonomous": True,
                "sources": {"prebuilt_flat_root": "data/x"},
                "train": {"backend": "cloud", "allow_leaky_val": True},
            }
        )
        self.assertTrue(cfg.self_train.enabled)
        self.assertTrue(cfg.self_train.retrain)
        self.assertTrue(cfg.self_train.conf_schedule)
        self.assertTrue(cfg.self_train.use_teacher)
        self.assertEqual(cfg.self_train.iterations, 3)
        self.assertTrue(cfg.self_train.autonomous)
        self.assertTrue(cfg.label.enabled)
        self.assertFalse(cfg.human_gate.enabled)
        self.assertFalse(cfg.human_gate.block)

    def test_human_gate_block_with_autonomous_is_config_error(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "gate_auto",
                    "autonomous": True,
                    "sources": {"prebuilt_flat_root": "data/x"},
                    "train": {"backend": "cloud", "allow_leaky_val": True},
                    "human_gate": {"enabled": True, "block": True},
                }
            )
        self.assertIn("human_gate.block", str(ctx.exception).lower())

    def test_human_gate_block_with_iterations_gt1_is_config_error(self) -> None:
        with self.assertRaises(AutoTrainConfigError) as ctx:
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "gate_iters",
                    "sources": {"prebuilt_flat_root": "data/x"},
                    "train": {"backend": "cloud", "allow_leaky_val": True},
                    "self_train": {
                        "enabled": True,
                        "iterations": 2,
                    },
                    "human_gate": {"enabled": True, "block": True},
                }
            )
        msg = str(ctx.exception).lower()
        self.assertTrue(
            "human_gate" in msg or "iterations" in msg,
            msg,
        )

    def test_conf_schedule_recorded_in_history(self) -> None:
        """conf_schedule grows conf; history stores per-iter conf."""
        from cs2_vision_access.training.self_train import SelfTrainReport

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            base = 0.5
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "st_conf_sched",
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
                        "iterations": 2,
                        "conf_threshold": base,
                        "conf_schedule": True,
                    },
                }
            )
            seen_confs: list[float] = []

            def fake_self_train(*args, **kwargs):
                conf = float(kwargs["conf_threshold"])
                seen_confs.append(conf)
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
                    conf_threshold=conf,
                    labels_written=("a.txt",),
                    report_path=str(report_path),
                )

            with patch(
                "cs2_vision_access.training.self_train.run_self_train_iteration",
                side_effect=fake_self_train,
            ):
                result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(len(seen_confs), 2)
            self.assertAlmostEqual(seen_confs[0], base)
            expected_it2 = min(0.85, base * (1 + 0.05 * (2 - 1)))
            self.assertAlmostEqual(seen_confs[1], expected_it2)
            history = result.state.artifacts.get("self_train_history") or []
            self.assertEqual(len(history), 2)
            self.assertAlmostEqual(float(history[0]["conf"]), base)
            self.assertAlmostEqual(float(history[1]["conf"]), expected_it2)
            notes = result.state.artifacts.get("self_train_notes") or []
            self.assertTrue(
                any("conf_schedule" in str(n) for n in notes),
                f"expected conf_schedule note, got {notes!r}",
            )


if __name__ == "__main__":
    unittest.main()
