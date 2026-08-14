"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.training.auto import (
    AutoTrainConfigError,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from cs2_vision_access.training.auto.paths import build_run_paths
from cs2_vision_access.training.auto.state import StageState, load_state, save_state
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
)


class LabelBootstrapStageTests(unittest.TestCase):
    def test_label_config_defaults_and_parse(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "session_split",
                "run_id": "lblcfg",
                "sources": {"prebuilt_dataset_root": "data/x"},
                "train": {"backend": "local"},
            }
        )
        self.assertEqual(cfg.label.bootstrap_splits, ("train",))
        self.assertAlmostEqual(cfg.label.min_label_ratio, 0.05)
        self.assertFalse(cfg.label.required)
        payload = cfg.to_dict()["label"]
        self.assertEqual(payload["bootstrap_splits"], ["train"])
        self.assertAlmostEqual(payload["min_label_ratio"], 0.05)

        cfg2 = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "session_split",
                "run_id": "lblcfg2",
                "sources": {"prebuilt_dataset_root": "data/x"},
                "train": {"backend": "local"},
                "label": {
                    "bootstrap_splits": ["train", "val"],
                    "min_label_ratio": 0.1,
                    "required": True,
                },
            }
        )
        self.assertEqual(cfg2.label.bootstrap_splits, ("train", "val"))
        self.assertAlmostEqual(cfg2.label.min_label_ratio, 0.1)
        self.assertTrue(cfg2.label.required)

    def test_session_split_sparse_bootstraps_train_only(self) -> None:
        from cs2_vision_access.training.auto.stages import stage_label

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "split"
            # session_split layout: many train images, almost no labels
            for split in ("train", "val"):
                (data / "images" / split).mkdir(parents=True, exist_ok=True)
                (data / "labels" / split).mkdir(parents=True, exist_ok=True)
            for i in range(5):
                (data / "images" / "train" / f"t{i}.png").write_bytes(b"img")
            (data / "images" / "val" / "v0.png").write_bytes(b"img")
            # One empty-ish coverage far below min_label_ratio
            # (0 labels / 5 images)

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "boot_sess",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "local",
                        "class_names": {"0": "ct", "1": "t"},
                        "min_labels": 1,
                    },
                    "label": {
                        "enabled": True,
                        "bootstrap": True,
                        "bootstrap_splits": ["train"],
                        "min_label_ratio": 0.05,
                    },
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            calls: list[dict[str, object]] = []

            def fake_bootstrap(data_dir, **kwargs):  # type: ignore[no-untyped-def]
                calls.append({"data_dir": Path(data_dir), **kwargs})
                # Simulate writing one train label so post-check can pass soft path.
                labels_dir = Path(kwargs["labels_dir"])
                labels_dir.mkdir(parents=True, exist_ok=True)
                (labels_dir / "t0.txt").write_text("0 0.1 0.1 0.9 0.1 0.5 0.9\n", encoding="utf-8")
                return 1

            with patch(
                "cs2_vision_access.training.bootstrap_labels.bootstrap_labels_with_yolo_person",
                side_effect=fake_bootstrap,
            ):
                updates = stage_label(cfg, paths, state)

            self.assertEqual(updates.get("label_status"), "bootstrapped")
            self.assertEqual(len(calls), 1)
            call = calls[0]
            self.assertEqual(Path(str(call["images_dir"])), data / "images" / "train")
            self.assertEqual(Path(str(call["labels_dir"])), data / "labels" / "train")
            self.assertFalse(call.get("write_yaml"))
            self.assertEqual(call.get("classes"), {0: "ct", 1: "t"})
            # Val must not be bootstrapped by default.
            self.assertNotEqual(Path(str(call["images_dir"])), data / "images" / "val")
            # Session yaml rewritten with multi-class names.
            yaml_path = data / "dataset.yaml"
            self.assertTrue(yaml_path.is_file())
            yaml_text = yaml_path.read_text(encoding="utf-8")
            self.assertIn("images/train", yaml_text)
            self.assertIn("ct", yaml_text)
            self.assertIn("t", yaml_text)
            self.assertNotIn("player", yaml_text)

    def test_flat_sparse_still_bootstraps(self) -> None:
        from cs2_vision_access.training.auto.stages import stage_label

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "flat"
            (data / "images").mkdir(parents=True)
            (data / "labels").mkdir(parents=True)
            for i in range(5):
                (data / "images" / f"f{i}.jpg").write_bytes(b"img")
            # no labels → sparse

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "boot_flat",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "min_labels": 1,
                    },
                    "label": {"enabled": True, "bootstrap": True},
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            calls: list[dict[str, object]] = []

            def fake_bootstrap(data_dir, **kwargs):  # type: ignore[no-untyped-def]
                calls.append({"data_dir": Path(data_dir), **kwargs})
                (data / "labels" / "f0.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
                return 1

            with patch(
                "cs2_vision_access.training.bootstrap_labels.bootstrap_labels_with_yolo_person",
                side_effect=fake_bootstrap,
            ):
                updates = stage_label(cfg, paths, state)

            self.assertEqual(updates.get("label_status"), "bootstrapped")
            self.assertEqual(len(calls), 1)
            # Flat path uses data_dir root; no images_dir override required.
            self.assertEqual(calls[0]["data_dir"], data)
            self.assertNotIn("images_dir", calls[0] or {})
            # kwargs may still have images_dir=None only if passed; our call omits it.
            self.assertIsNone(calls[0].get("images_dir"))
            self.assertIsNone(calls[0].get("labels_dir"))

    def test_session_split_required_hard_fails_when_still_sparse(self) -> None:
        from cs2_vision_access.training.auto.stages import (
            AutoTrainStageError,
            stage_label,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "split"
            for split in ("train", "val"):
                (data / "images" / split).mkdir(parents=True, exist_ok=True)
                (data / "labels" / split).mkdir(parents=True, exist_ok=True)
            for i in range(10):
                (data / "images" / "train" / f"t{i}.png").write_bytes(b"img")

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "boot_req",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {"backend": "local", "min_labels": 3},
                    "label": {
                        "bootstrap": True,
                        "required": True,
                        "min_label_ratio": 0.5,
                    },
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            def fake_bootstrap(*_a, **_k):  # type: ignore[no-untyped-def]
                return 0  # wrote nothing → still sparse

            with (
                patch(
                    "cs2_vision_access.training.bootstrap_labels.bootstrap_labels_with_yolo_person",
                    side_effect=fake_bootstrap,
                ),
                self.assertRaises(AutoTrainStageError) as ctx,
            ):
                stage_label(cfg, paths, state)
            self.assertIn("label.required", str(ctx.exception).lower())

    def test_label_teacher_edgesam_config_roundtrip(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "session_split",
                "run_id": "edgcfg",
                "sources": {"prebuilt_dataset_root": "data/x"},
                "train": {"backend": "local"},
                "label": {
                    "teacher": "edgesam",
                    "artifacts_dir": "artifacts",
                    "edgesam_confidence": 0.5,
                    "sample_rate": 2.0,
                    "max_frames": 10,
                    "collapse_to_player": True,
                    "keep_negatives": False,
                    "coco_fallback": False,
                },
            }
        )
        self.assertEqual(cfg.label.teacher, "edgesam")
        self.assertEqual(cfg.label.artifacts_dir, Path("artifacts"))
        self.assertAlmostEqual(cfg.label.edgesam_confidence, 0.5)
        self.assertAlmostEqual(cfg.label.sample_rate, 2.0)
        self.assertEqual(cfg.label.max_frames, 10)
        self.assertTrue(cfg.label.collapse_to_player)
        self.assertFalse(cfg.label.keep_negatives)
        self.assertFalse(cfg.label.coco_fallback)
        self.assertFalse(cfg.label.download_edgesam)
        payload = cfg.to_dict()["label"]
        self.assertEqual(payload["teacher"], "edgesam")
        self.assertEqual(payload["artifacts_dir"], "artifacts")
        self.assertFalse(payload["coco_fallback"])
        self.assertFalse(payload["download_edgesam"])

        with self.assertRaises(AutoTrainConfigError):
            config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "badteacher",
                    "sources": {"prebuilt_dataset_root": "data/x"},
                    "train": {"backend": "local"},
                    "label": {"teacher": "unknown"},
                }
            )

    def test_edgesam_flat_sparse_calls_prepare_lib(self) -> None:
        from cs2_vision_access.training.auto.stages import stage_label
        from cs2_vision_access.training.prepare_lib import PrepareResult

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "flat"
            (data / "images").mkdir(parents=True)
            (data / "labels").mkdir(parents=True)
            for i in range(5):
                (data / "images" / f"f{i}.jpg").write_bytes(b"img")

            art = root / "artifacts"
            for name in (
                "yolov10n_cs2_fp16.onnx",
                "yolov10n_cs2_fp16.model.json",
                "edge_sam_3x_encoder.onnx",
                "edge_sam_3x_decoder.onnx",
            ):
                p = art / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(b"x")

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "edg_flat",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "min_labels": 1,
                    },
                    "label": {
                        "enabled": True,
                        "bootstrap": True,
                        "teacher": "edgesam",
                        "artifacts_dir": str(art),
                        "coco_fallback": False,
                    },
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            calls: list[dict[str, object]] = []

            def fake_prepare(**kwargs):  # type: ignore[no-untyped-def]
                calls.append(dict(kwargs))
                (data / "labels" / "f0.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
                return PrepareResult(labeled_frames=1, write_root=str(data))

            with patch(
                # bootstrap imports run_cs2_sam_prepare from prepare_run; patch there.
                "cs2_vision_access.training.prepare_lib.bootstrap.run_cs2_sam_prepare",
                side_effect=fake_prepare,
            ):
                updates = stage_label(cfg, paths, state)

            self.assertEqual(updates.get("label_status"), "edgesam")
            self.assertEqual(updates.get("label_teacher"), "edgesam")
            self.assertEqual(len(calls), 1)
            self.assertEqual(Path(str(calls[0]["images_dir"])), data / "images")
            # labels_dir may be omitted (prepare_lib resolves sibling labels/)
            if "labels_dir" in calls[0] and calls[0]["labels_dir"] is not None:
                self.assertEqual(Path(str(calls[0]["labels_dir"])), data / "labels")
            self.assertEqual(updates.get("edgesam_labeled"), 1)

    def test_coco_teacher_still_works(self) -> None:
        from cs2_vision_access.training.auto.stages import stage_label

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "flat"
            (data / "images").mkdir(parents=True)
            (data / "labels").mkdir(parents=True)
            for i in range(5):
                (data / "images" / f"f{i}.jpg").write_bytes(b"img")

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "coco_still",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "min_labels": 1,
                    },
                    "label": {
                        "enabled": True,
                        "bootstrap": True,
                        "teacher": "coco_person",
                    },
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            def fake_bootstrap(data_dir, **kwargs):  # type: ignore[no-untyped-def]
                (data / "labels" / "f0.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
                return 1

            with patch(
                "cs2_vision_access.training.bootstrap_labels.bootstrap_labels_with_yolo_person",
                side_effect=fake_bootstrap,
            ):
                updates = stage_label(cfg, paths, state)

            self.assertEqual(updates.get("label_status"), "bootstrapped")
            self.assertEqual(updates.get("label_teacher"), "coco_person")

    def test_edgesam_missing_assets_soft_skip(self) -> None:
        from cs2_vision_access.training.auto.stages import stage_label

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "flat"
            (data / "images").mkdir(parents=True)
            (data / "labels").mkdir(parents=True)
            for i in range(3):
                (data / "images" / f"f{i}.jpg").write_bytes(b"img")

            empty_art = root / "empty_artifacts"
            empty_art.mkdir()

            work = root / "auto"
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "edg_miss",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "min_labels": 1,
                    },
                    "label": {
                        "enabled": True,
                        "bootstrap": False,
                        "teacher": "edgesam",
                        "artifacts_dir": str(empty_art),
                        "coco_fallback": False,
                    },
                }
            )
            paths = build_run_paths(cfg.paths.work_root, cfg.run_id)
            state = StageState(run_id=cfg.run_id, mode=cfg.mode)
            state.artifacts["dataset_root"] = str(data)

            updates = stage_label(cfg, paths, state)
            self.assertEqual(updates.get("label_status"), "skipped_edgesam_assets")
            self.assertEqual(updates.get("label_teacher"), "edgesam")
            notes = " ".join(str(n) for n in (updates.get("label_notes") or []))
            self.assertIn("assets", notes.lower())

    def test_autonomous_does_not_force_edgesam_teacher(self) -> None:
        cfg = config_from_mapping(
            {
                "schema_version": 1,
                "mode": "flat_cloud",
                "run_id": "auto_teacher",
                "autonomous": True,
                "sources": {"prebuilt_flat_root": "data/flat"},
                "train": {"backend": "cloud", "allow_leaky_val": True},
            }
        )
        self.assertTrue(cfg.label.enabled)
        self.assertTrue(cfg.label.bootstrap)
        self.assertEqual(cfg.label.teacher, "coco_person")


class ClassIdGateTests(unittest.TestCase):
    def test_validate_rejects_incompatible_class_ids(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            (data / "labels" / "frame.txt").write_text("5 0.5 0.5 0.1 0.1\n", encoding="utf-8")
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "badclass",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                }
            )
            with self.assertRaises(AutoTrainError) as ctx:
                run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertIn("class id", str(ctx.exception).lower())


class ArtifactScrubTests(unittest.TestCase):
    def test_from_stage_scrubs_downstream_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "state.json"
            state = StageState(run_id="r", mode="flat_cloud")
            state.mark_completed(
                "ingest",
                updates={"dataset_root": "/x", "ingest_source": "prebuilt"},
            )
            state.mark_completed("prepare_data")
            state.mark_completed("label", updates={"label_status": "sufficient"})
            state.mark_completed("validate")
            state.mark_completed(
                "train",
                updates={
                    "onnx_model": "/old.onnx",
                    "manifest": "/old.json",
                    "train_metrics": {"mAP50": 0.1},
                },
            )
            state.mark_completed(
                "export",
                updates={"smoke_ok": True, "package_zip": "/old.zip"},
            )
            save_state(path, state)
            loaded = load_state(path)
            assert loaded is not None
            stages = loaded.stages_to_run(from_stage="train")
            self.assertEqual(stages, ["train", "export", "self_train", "eval", "report"])
            self.assertEqual(loaded.artifacts.get("dataset_root"), "/x")
            self.assertNotIn("onnx_model", loaded.artifacts)
            self.assertNotIn("train_metrics", loaded.artifacts)
            self.assertNotIn("smoke_ok", loaded.artifacts)
            self.assertNotIn("package_zip", loaded.artifacts)


class SoftNotesDegradedTests(unittest.TestCase):
    def test_soft_notes_mark_report_degraded(self) -> None:
        from cs2_vision_access.training.auto.report import resolve_report_status

        status = resolve_report_status(
            "completed",
            {"soft_notes": ["progress report skipped: boom"]},
        )
        self.assertEqual(status, "degraded")


if __name__ == "__main__":
    unittest.main()
