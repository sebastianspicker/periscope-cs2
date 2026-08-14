"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from cs2_vision_access.training.auto import (
    EXIT_OK,
    STAGE_ORDER,
    AutoTrainError,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from cs2_vision_access.training.auto.state import StageState, load_state, save_state
from cs2_vision_access.training.local import TrainingSummary
from tests.train_auto_helpers import (
    POLYGON,
    _mock_cloud_train,
    _mock_local_train,
    _write_flat_dataset,
    _write_flat_zip,
    _write_split_dataset,
)


class StateResumeTests(unittest.TestCase):
    def test_atomic_save_and_stages_to_run(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "state.json"
            state = StageState(run_id="r", mode="flat_cloud")
            state.mark_completed("ingest", updates={"dataset_root": "/x"})
            state.mark_completed("prepare_data")
            save_state(path, state)
            loaded = load_state(path)
            assert loaded is not None
            self.assertEqual(loaded.completed_stages, ["ingest", "prepare_data"])
            remaining = loaded.stages_to_run()
            self.assertEqual(
                remaining,
                ["label", "validate", "train", "export", "self_train", "eval", "report"],
            )
            from_train = loaded.stages_to_run(from_stage="train")
            self.assertEqual(from_train, ["train", "export", "self_train", "eval", "report"])
            self.assertEqual(loaded.completed_stages, ["ingest", "prepare_data"])


class FlatCloudPipelineTests(unittest.TestCase):
    def test_prebuilt_flat_with_mocked_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "flat1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                        "smoke": True,
                    },
                    "export": {"promote_to_run_models": True},
                    "resume": {"enabled": True},
                    "eval": {"enabled": False},
                }
            )
            result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(result.state.status, "completed")
            self.assertEqual(result.state.completed_stages, list(STAGE_ORDER))
            self.assertEqual(result.state.artifacts.get("label_status"), "sufficient")
            models = work / "flat1" / "models"
            self.assertTrue((models / "cs2-yolo11n-seg.onnx").is_file())
            self.assertTrue((models / "cs2-yolo11n-seg.model.json").is_file())
            package = work / "flat1" / "package" / "model-package.zip"
            self.assertTrue(package.is_file())
            report = work / "flat1" / "report.json"
            self.assertTrue(report.is_file())
            payload = json.loads(report.read_text(encoding="utf-8"))
            # Smoke/eval may soft-fail on fake ONNX → ok or degraded.
            self.assertIn(payload["status"], {"ok", "degraded"})
            self.assertIn("progress_report_md", payload)
            self.assertIn("metrics", payload)

    def test_zip_extract_real_and_mocked_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            zip_path = root / "bundle.zip"
            _write_flat_zip(zip_path)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "zip1",
                    "paths": {"work_root": str(work)},
                    "sources": {"dataset_zip": str(zip_path)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "export": {"promote_to_run_models": True},
                    "eval": {"enabled": False},
                }
            )
            result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            extracted = work / "zip1" / "data" / "extracted"
            self.assertTrue((extracted / "images" / "frame.jpg").is_file())
            self.assertTrue((extracted / "labels" / "frame.txt").is_file())

    def test_refuse_leaky_val_without_flag(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "leaky",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": False,
                        "class_names": {"0": "player"},
                    },
                }
            )
            with self.assertRaises(AutoTrainError) as ctx:
                run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertIn("leaky", str(ctx.exception).lower())
            state = load_state(work / "leaky" / "state.json")
            assert state is not None
            self.assertEqual(state.status, "failed")
            # validate failed — stage must not be marked completed
            self.assertNotIn("validate", state.completed_stages)
            self.assertEqual(state.current_stage, "validate")
            self.assertIn("label", state.completed_stages)

    def test_resume_skips_completed_stages(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "resume1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                    },
                    "eval": {"enabled": False},
                }
            )
            train_calls: list[int] = []

            def counting_train(data_dir: Path, **kwargs: object) -> Path:
                train_calls.append(1)
                return _mock_cloud_train(data_dir, **kwargs)

            first = run_auto_train(cfg, train_cloud_fn=counting_train)
            self.assertEqual(first.exit_code, EXIT_OK)
            self.assertEqual(len(train_calls), 1)
            second = run_auto_train(cfg, train_cloud_fn=counting_train)
            self.assertEqual(second.exit_code, EXIT_OK)
            self.assertEqual(len(train_calls), 1, "train must not re-run when complete")

    def test_cloud_train_receives_resume_and_project_kwargs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "kwargs1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                        "resume_ultralytics": True,
                    },
                    "resume": {"enabled": True},
                    "eval": {"enabled": False},
                }
            )
            seen: dict[str, object] = {}

            def capturing_train(data_dir: Path, **kwargs: object) -> Path:
                seen.update(kwargs)
                return _mock_cloud_train(data_dir, **kwargs)

            # Seed last.pt so resume flag becomes True.
            last = work / "kwargs1" / "train_runs" / "kwargs1" / "weights" / "last.pt"
            last.parent.mkdir(parents=True, exist_ok=True)
            last.write_bytes(b"ckpt")

            result = run_auto_train(cfg, train_cloud_fn=capturing_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(Path(str(seen["project"])), work / "kwargs1" / "train_runs")
            self.assertEqual(seen["run_name"], "kwargs1")
            self.assertTrue(seen["resume"])
            self.assertTrue(seen["plots"])
            self.assertTrue(result.state.artifacts.get("train_resume"))


class SessionSplitPipelineTests(unittest.TestCase):
    def test_prebuilt_split_with_mocked_local_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "split"
            class_names = {0: "ct", 1: "t"}
            _write_split_dataset(data, class_names=class_names)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "sess1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "local",
                        "class_names": {str(k): v for k, v in class_names.items()},
                        "smoke": True,
                        "device": "cpu",
                    },
                    "export": {"promote_to_run_models": True},
                    "eval": {"enabled": False},
                }
            )
            captured: dict[str, object] = {}

            def local_train(**kwargs: object) -> TrainingSummary:
                captured.update(kwargs)
                run_dir = Path(str(kwargs["project_directory"])) / str(kwargs["run_name"])
                return _mock_local_train(out_dir=run_dir, **kwargs)

            result = run_auto_train(cfg, train_local_fn=local_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            promoted = work / "sess1" / "models" / "best.onnx"
            self.assertTrue(promoted.is_file())
            self.assertEqual(
                Path(str(captured["project_directory"])),
                work / "sess1" / "train_runs",
            )
            self.assertEqual(captured["run_name"], "sess1")
            self.assertTrue(captured.get("exist_ok"))
            # No last.pt yet → resume False
            self.assertFalse(captured.get("resume"))
            # Smoke uses fake images/onnx → may fail soft; not skipped_mvp.
            self.assertNotEqual(
                result.state.artifacts.get("smoke_inference"),
                "skipped_mvp",
            )
            self.assertIn(
                result.state.artifacts.get("smoke_inference"),
                {
                    "ok",
                    "failed",
                    "import_failed",
                    "skipped_no_frame",
                    "skipped_no_source",
                },
            )
            # Progress report path stored when writable.
            self.assertTrue(
                result.state.artifacts.get("progress_report_md")
                or "soft_notes" in result.state.artifacts
            )

    def test_local_train_resume_when_last_pt_exists(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "split"
            _write_split_dataset(data, class_names={0: "ct"})
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "resume_local",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "local",
                        "class_names": {"0": "ct"},
                        "smoke": True,
                        "resume_ultralytics": True,
                    },
                    "resume": {"enabled": True},
                    "eval": {"enabled": False},
                }
            )
            last = work / "resume_local" / "train_runs" / "resume_local" / "weights" / "last.pt"
            last.parent.mkdir(parents=True, exist_ok=True)
            last.write_bytes(b"ckpt")
            captured: dict[str, object] = {}

            def local_train(**kwargs: object) -> TrainingSummary:
                captured.update(kwargs)
                run_dir = Path(str(kwargs["project_directory"])) / str(kwargs["run_name"])
                return _mock_local_train(out_dir=run_dir, **kwargs)

            result = run_auto_train(cfg, train_local_fn=local_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertTrue(captured.get("resume"))
            self.assertTrue(captured.get("exist_ok"))
            self.assertTrue(result.state.artifacts.get("train_resume"))

    def test_assemble_from_staging_and_plan(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            staging = root / "staging"
            (staging / "s-train").mkdir(parents=True)
            (staging / "s-val").mkdir(parents=True)
            for session, stem in (("s-train", "a"), ("s-val", "b")):
                (staging / session / f"{stem}.png").write_bytes(b"image")
                (staging / session / f"{stem}.txt").write_text(
                    POLYGON if session == "s-train" else "",
                    encoding="utf-8",
                )
            plan_path = root / "plan.json"
            plan_path.write_text(
                json.dumps({"train": ["s-train"], "val": ["s-val"]}),
                encoding="utf-8",
            )
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "assemble1",
                    "paths": {"work_root": str(work)},
                    "sources": {
                        "staging_root": str(staging),
                        "split_plan": str(plan_path),
                    },
                    "train": {
                        "backend": "local",
                        "class_names": {"0": "ct"},
                    },
                    "eval": {"enabled": False},
                }
            )

            def local_train(**kwargs: object) -> TrainingSummary:
                run_dir = Path(str(kwargs["project_directory"])) / str(kwargs["run_name"])
                return _mock_local_train(out_dir=run_dir, **kwargs)

            result = run_auto_train(cfg, train_local_fn=local_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            dataset = work / "assemble1" / "data" / "dataset"
            self.assertTrue((dataset / "images" / "train" / "s-train" / "a.png").is_file())
            self.assertTrue((dataset / "images" / "val" / "s-val" / "b.png").is_file())
            self.assertTrue((dataset / "dataset.yaml").is_file())

    def test_min_map50_gate_fails_when_below_threshold(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "split"
            _write_split_dataset(data, class_names={0: "ct"})
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "mapgate",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "local",
                        "class_names": {"0": "ct"},
                        "smoke": True,
                        "min_map50": 0.9,
                    },
                    "eval": {"enabled": False},
                }
            )

            def local_train(**kwargs: object) -> TrainingSummary:
                run_dir = Path(str(kwargs["project_directory"])) / str(kwargs["run_name"])
                return _mock_local_train(out_dir=run_dir, **kwargs)

            with self.assertRaises(AutoTrainError) as ctx:
                run_auto_train(cfg, train_local_fn=local_train)
            self.assertIn("min_map50", str(ctx.exception).lower())


class SessionSplitCloudTests(unittest.TestCase):
    def test_materialize_flat_cloud_bundle(self) -> None:
        from cs2_vision_access.training.auto.cloud_bundle import (
            materialize_flat_cloud_bundle,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            split = root / "split"
            _write_split_dataset(split, class_names={0: "ct", 1: "t"})
            # provenance file should be carried into the bundle when present
            (split / "sessions.json").write_text(
                json.dumps({"s-train": "s-train", "s-val": "s-val"}),
                encoding="utf-8",
            )
            cloud_dir = root / "cloud"

            flat_root, zip_path = materialize_flat_cloud_bundle(
                split,
                cloud_dir,
                class_names={0: "ct", 1: "t"},
            )

            self.assertEqual(flat_root, cloud_dir / "dataset")
            self.assertEqual(zip_path, cloud_dir / "dataset.zip")
            # flat tree has every labeled train/val image + label
            self.assertTrue((flat_root / "images" / "train_a.png").is_file())
            self.assertTrue((flat_root / "images" / "val_b.png").is_file())
            self.assertTrue((flat_root / "labels" / "train_a.txt").is_file())
            self.assertTrue((flat_root / "labels" / "val_b.txt").is_file())
            self.assertTrue((flat_root / "sessions.json").is_file())
            # flat dataset.yaml (train=val=images) for cloud.train
            yaml_text = (flat_root / "dataset.yaml").read_text(encoding="utf-8")
            self.assertIn("train: images", yaml_text)
            self.assertIn("val: images", yaml_text)
            self.assertIn("ct", yaml_text)
            # zip carries yaml, flat images/labels, and provenance
            with zipfile.ZipFile(zip_path) as zf:
                names = set(zf.namelist())
                self.assertIn("dataset.yaml", names)
                self.assertIn("images/train_a.png", names)
                self.assertIn("images/val_b.png", names)
                self.assertIn("labels/train_a.txt", names)
                self.assertIn("labels/val_b.txt", names)
                self.assertIn("sessions.json", names)
            self.assertTrue(zip_path.is_file())

    def test_materialize_flat_cloud_bundle_rejects_non_split(self) -> None:
        from cs2_vision_access.training.auto.cloud_bundle import (
            materialize_flat_cloud_bundle,
        )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            flat = root / "flat"
            (flat / "images").mkdir(parents=True)
            (flat / "labels").mkdir(parents=True)
            (flat / "images" / "a.png").write_bytes(b"image")
            (flat / "labels" / "a.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                materialize_flat_cloud_bundle(
                    flat,
                    root / "cloud",
                    class_names={0: "player"},
                )

    def test_session_split_cloud_pipeline_with_mocked_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "split"
            class_names = {0: "ct", 1: "t"}
            _write_split_dataset(data, class_names=class_names)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "session_split",
                    "run_id": "sess_cloud",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_dataset_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "class_names": {str(k): v for k, v in class_names.items()},
                    },
                    "export": {"promote_to_run_models": True},
                    "eval": {"enabled": False},
                }
            )
            cloud_dirs: list[Path] = []

            def capturing_cloud_train(data_dir, **kwargs):  # type: ignore[no-untyped-def]
                cloud_dirs.append(Path(data_dir))
                return _mock_cloud_train(data_dir, **kwargs)

            result = run_auto_train(cfg, train_cloud_fn=capturing_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            self.assertEqual(result.state.artifacts.get("train_backend"), "cloud")
            # cloud train received the flattened root, not the split tree
            flat_root = Path(str(result.state.artifacts["cloud_dataset_root"]))
            self.assertEqual(cloud_dirs[0], flat_root)
            self.assertTrue((flat_root / "images" / "train_a.png").is_file())
            self.assertTrue((flat_root / "dataset.yaml").is_file())
            # zip bundle produced under the run dir
            zip_path = work / "sess_cloud" / "cloud" / "dataset.zip"
            self.assertTrue(zip_path.is_file())
            self.assertEqual(
                Path(str(result.state.artifacts["cloud_dataset_zip"])).resolve(),
                zip_path.resolve(),
            )
            # split tree (validation/progress) still used as dataset_root
            dataset_root = Path(str(result.state.artifacts["dataset_root"]))
            self.assertTrue((dataset_root / "images" / "train").is_dir())
            # export promoted the cloud ONNX + manifest into run models/
            self.assertTrue((work / "sess_cloud" / "models" / "cs2-yolo11n-seg.onnx").is_file())
            self.assertTrue(
                (work / "sess_cloud" / "models" / "cs2-yolo11n-seg.model.json").is_file()
            )


if __name__ == "__main__":
    unittest.main()
