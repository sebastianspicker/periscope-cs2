"""Tests for remote autonomous training loop."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.training.remote_autonomous import (
    AutonomousReport,
    run_autonomous_loop,
)
from tests.remote_autonomous_helpers import AutonomousLoopFakesMixin


class AutonomousLoopTests(AutonomousLoopFakesMixin, unittest.TestCase):
    def test_loop_train_and_self_train_mocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=self._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=2,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                )

            self.assertIsInstance(report, AutonomousReport)
            self.assertEqual(len(report.iterations), 2)
            self.assertTrue(Path(report.onnx_path).is_file())
            self.assertTrue((data / "autonomous_report.json").is_file())
            payload = json.loads((data / "autonomous_report.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["schema_version"], 1)

            # Held-out dirs created when enough images (default allow_leaky_val=False)
            self.assertTrue((data / "images" / "train").is_dir())
            self.assertTrue((data / "images" / "val").is_dir())
            self.assertTrue((data / "labels" / "train").is_dir())
            self.assertTrue((data / "labels" / "val").is_dir())
            self.assertGreater(len(list((data / "images" / "train").glob("*.jpg"))), 0)
            self.assertGreater(len(list((data / "images" / "val").glob("*.jpg"))), 0)

            # Best-iteration packaging fields
            self.assertIsNotNone(getattr(report, "best_iteration", None))
            self.assertIn("best_iteration", payload)
            self.assertEqual(report.best_iteration, 2)  # higher mAP on iter_2
            self.assertIsNotNone(report.best_map)
            self.assertAlmostEqual(float(report.best_map), 0.7, places=5)
            self.assertEqual(report.status, "ok")

            # Progress report wiring
            progress_md = data / "progress" / "report.md"
            self.assertTrue(
                progress_md.is_file(),
                "expected data/progress/report.md after run_autonomous_loop",
            )
            if getattr(report, "progress_dir", None) is not None:
                self.assertTrue(Path(report.progress_dir).is_dir())
            if getattr(report, "progress_report_md", None) is not None:
                self.assertTrue(Path(report.progress_report_md).is_file())
            self.assertTrue((data / "progress" / "metrics_iter_01.json").is_file())
            self.assertTrue((data / "progress" / "metrics_iter_02.json").is_file())
            self.assertTrue((data / "runs" / "iter_1" / "results.csv").is_file())
            self.assertTrue((data / "runs" / "iter_2" / "results.csv").is_file())

            # dataset.yaml uses session_split paths
            yaml_text = (data / "dataset.yaml").read_text(encoding="utf-8")
            self.assertIn("images/train", yaml_text)
            self.assertIn("images/val", yaml_text)

            # State file for mid-loop resume
            self.assertTrue((data / "autonomous_state.json").is_file())

    def test_loop_allow_leaky_val_flat_layout(self) -> None:
        """Backward-compat: allow_leaky_val=True keeps flat train=val layout."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=3, labeled=1)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=self._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    allow_leaky_val=True,
                    resume=False,
                )

            self.assertIsInstance(report, AutonomousReport)
            self.assertEqual(len(report.iterations), 1)
            yaml_text = (data / "dataset.yaml").read_text(encoding="utf-8")
            self.assertIn("train: images", yaml_text)
            # Flat layout: no requirement that train/val subdirs exist
            self.assertFalse(
                (data / "images" / "train").is_dir() and any((data / "images" / "train").iterdir())
            )
            self.assertIsNotNone(getattr(report, "best_iteration", None))

    def test_package_failure_sets_status_degraded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=self._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=RuntimeError("zip failed"),
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                )

            self.assertEqual(report.status, "degraded")
            self.assertTrue(
                any("package_outputs" in n for n in report.notes),
                report.notes,
            )
            self.assertIsNone(report.bundle_path)

    def test_resume_restores_iteration_reports_and_progress(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)

            # Pre-seed autonomous_state as if iter 1 finished.
            state = {
                "completed_iters": 1,
                "best_iteration": 1,
                "best_map": 0.5,
                "best_onnx_path": None,
                "best_manifest_path": None,
                "batch": 2,
                "notes": ["seeded"],
                "status": "ok",
                "iteration_reports": [
                    {
                        "iteration": 1,
                        "onnx": "cs2-yolo11n-seg.onnx",
                        "manifest": "cs2-yolo11n-seg.model.json",
                        "labeled_before": 3,
                        "labeled_after": 3,
                        "self_train_accepted": 0,
                        "self_train_rejected_low_conf": 0,
                        "map50": 0.5,
                        "map50_95": None,
                    }
                ],
                "progress_iterations": [
                    {
                        "iteration": 1,
                        "labeled_before": 3,
                        "labeled_after": 3,
                        "self_train_accepted": 0,
                        "self_train_rejected_low_conf": 0,
                        "map50": 0.5,
                        "map50_95": None,
                    }
                ],
            }
            # Need held-out split + yaml so loop can resume into iter 2.
            # run_autonomous_loop will re-materialize split when not leaky.
            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=self._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                (data / "autonomous_state.json").write_text(
                    __import__("json").dumps(state), encoding="utf-8"
                )
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=2,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=True,
                )

            # Resumed: prior iter 1 + new iter 2.
            self.assertEqual(len(report.iterations), 2)
            self.assertEqual(report.iterations[0].iteration, 1)
            self.assertEqual(report.iterations[1].iteration, 2)
            self.assertTrue(
                any("resume" in n for n in report.notes),
                report.notes,
            )
            # State on disk should still carry both series.
            saved = __import__("json").loads(
                (data / "autonomous_state.json").read_text(encoding="utf-8")
            )
            self.assertEqual(len(saved.get("iteration_reports", [])), 2)
            self.assertEqual(len(saved.get("progress_iterations", [])), 2)

    def test_best_iter_snapshots_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=self._fake_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=2,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                )

            self.assertTrue((data / "cs2-yolo11n-seg.iter1.onnx").is_file())
            self.assertTrue((data / "cs2-yolo11n-seg.iter2.onnx").is_file())
            self.assertTrue((data / "cs2-yolo11n-seg.iter1.model.json").is_file())
            self.assertTrue((data / "cs2-yolo11n-seg.iter2.model.json").is_file())
            self.assertEqual(report.best_iteration, 2)
            # Canonical names present for download.
            self.assertTrue((data / "cs2-yolo11n-seg.onnx").is_file())
            self.assertTrue((data / "cs2-yolo11n-seg.model.json").is_file())

    def test_self_train_teacher_passed_on_iter_2(self) -> None:
        """Iter 1 has no teacher; iter 2 receives prior best ONNX as teacher."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)
            calls: list[dict] = []

            def tracking_self_train(*a, **k):
                calls.append(dict(k))
                return self._fake_self_train(*a, **k)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=tracking_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=2,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                    use_teacher_gate=True,
                    teacher_min_iou=0.35,
                )

            self.assertEqual(len(report.iterations), 2)
            self.assertEqual(len(calls), 2)
            # Iter 1: no prior student ONNX → no teacher
            self.assertIsNone(calls[0].get("teacher_model"))
            self.assertIsNone(calls[0].get("teacher_manifest"))
            # Iter 2: prior best / last-iter ONNX must be passed
            self.assertIsNotNone(calls[1].get("teacher_model"))
            self.assertIsNotNone(calls[1].get("teacher_manifest"))
            self.assertAlmostEqual(float(calls[1]["teacher_min_iou"]), 0.35)
            teacher_path = Path(calls[1]["teacher_model"])
            self.assertTrue(teacher_path.is_file(), teacher_path)
            self.assertTrue(
                "iter1" in teacher_path.name or teacher_path.suffix == ".onnx",
                teacher_path.name,
            )
            self.assertTrue(
                any("self-train teacher=" in n for n in report.notes),
                report.notes,
            )

    def test_self_train_teacher_skipped_when_gate_disabled(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            self._seed_dataset(data, n=5, labeled=3)
            calls: list[dict] = []

            def tracking_self_train(*a, **k):
                calls.append(dict(k))
                return self._fake_self_train(*a, **k)

            with (
                patch("cs2_vision_access.training.remote_autonomous.deps.install_dependencies"),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.train",
                    side_effect=self._fake_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.create_manifest",
                    side_effect=self._fake_manifest,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.run_self_train_iteration",
                    side_effect=tracking_self_train,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.package_outputs",
                    side_effect=self._fake_package,
                ),
                patch(
                    "cs2_vision_access.training.remote_autonomous.deps.bootstrap_labels_with_yolo_person",
                    return_value=0,
                ),
            ):
                run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=2,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=False,
                    min_label_ratio=0.0,
                    resume=False,
                    use_teacher_gate=False,
                )

            self.assertEqual(len(calls), 2)
            for c in calls:
                self.assertIsNone(c.get("teacher_model"))
                self.assertIsNone(c.get("teacher_manifest"))


if __name__ == "__main__":
    unittest.main()
