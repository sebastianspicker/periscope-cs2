"""Tests for remote autonomous bootstrap (COCO person + EdgeSAM)."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.training.remote_autonomous import (
    bootstrap_labels_with_yolo_person,
    run_autonomous_loop,
)


class BootstrapTests(unittest.TestCase):
    def test_bootstrap_writes_person_polygons(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            # Minimal valid-ish image
            import cv2

            cv2.imwrite(str(images / "frame_00000000.jpg"), np.zeros((64, 64, 3), dtype=np.uint8))

            fake_result = MagicMock()
            fake_result.orig_shape = (64, 64)
            fake_result.masks = MagicMock()
            fake_result.masks.xy = [np.array([[10, 10], [50, 10], [50, 50], [10, 50]], dtype=float)]
            fake_result.boxes = None

            fake_model = MagicMock()
            fake_model.predict.return_value = [fake_result]

            with (
                patch(
                    "cs2_vision_access.training.remote_autonomous.YOLO",
                    create=True,
                ),
                patch("ultralytics.YOLO", return_value=fake_model),
            ):
                n = bootstrap_labels_with_yolo_person(root, device="cpu", conf=0.1)
            self.assertGreaterEqual(n, 1)
            label = labels / "frame_00000000.txt"
            self.assertTrue(label.is_file())
            text = label.read_text(encoding="utf-8")
            self.assertTrue(text.startswith("0 "), text)
            self.assertTrue((labels / "frame_00000000.txt.pseudo").is_file())
            self.assertTrue((root / "dataset.yaml").is_file())

    def test_bootstrap_skips_person_miss(self) -> None:
        """Person miss must leave frame unlabeled (no empty permanent label)."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            import cv2

            cv2.imwrite(str(images / "frame_00000000.jpg"), np.zeros((64, 64, 3), dtype=np.uint8))

            fake_model = MagicMock()
            fake_model.predict.return_value = []

            with patch("ultralytics.YOLO", return_value=fake_model):
                n = bootstrap_labels_with_yolo_person(root, device="cpu", conf=0.1)
            self.assertEqual(n, 0)
            self.assertFalse((labels / "frame_00000000.txt").is_file())


class EdgeSamBootstrapLoopTests(unittest.TestCase):
    """use_edgesam bootstrap path: success, fallback, and default COCO-only."""

    def _fake_train(self, data_dir, *a, **k):
        data_dir = Path(data_dir)
        onnx = data_dir / "cs2-yolo11n-seg.onnx"
        onnx.write_bytes(b"onnx")
        project = Path(k.get("project") or (data_dir / "runs"))
        name = k.get("run_name") or "train"
        run = Path(project) / name
        run.mkdir(parents=True, exist_ok=True)
        (run / "results.csv").write_text(
            "epoch,train/box_loss,metrics/mAP50(B)\n1,1.0,0.5\n",
            encoding="utf-8",
        )
        return onnx

    def _fake_manifest(self, onnx_path, data_dir, classes=None, **k):
        p = Path(data_dir) / "cs2-yolo11n-seg.model.json"
        p.write_text("{}", encoding="utf-8")
        return p

    def _fake_package(self, onnx_path, manifest_path, output_zip, **k):
        out = Path(output_zip)
        out.write_bytes(b"PK\x03\x04fake")
        return out

    def _fake_self_train(self, *a, **k):
        from cs2_vision_access.training.self_train import SelfTrainReport

        return SelfTrainReport(
            images_scanned=2,
            already_labeled=2,
            unlabeled_scanned=0,
            accepted=0,
            rejected_low_conf=0,
            rejected_empty=0,
            conf_threshold=0.45,
            labels_written=(),
        )

    def _seed_unlabeled(self, data: Path, n: int = 4) -> None:
        images = data / "images"
        labels = data / "labels"
        images.mkdir(parents=True)
        labels.mkdir()
        import cv2

        for i in range(n):
            cv2.imwrite(
                str(images / f"frame_{i:08d}.jpg"),
                np.zeros((32, 32, 3), dtype=np.uint8),
            )

    def test_use_edgesam_success_skips_coco(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp) / "data"
            self._seed_unlabeled(data, n=4)
            art = Path(tmp) / "art"
            art.mkdir()
            assets = {
                "detector": art / "det.onnx",
                "manifest": art / "det.model.json",
                "encoder": art / "enc.onnx",
                "decoder": art / "dec.onnx",
            }
            for path in assets.values():
                path.write_bytes(b"x")

            coco = MagicMock(return_value=0)

            def _prepare(**kwargs):
                images_dir = Path(kwargs["images_dir"])
                labels = data / "labels"
                labels.mkdir(exist_ok=True)
                n = 0
                for img in images_dir.glob("*.jpg"):
                    (labels / f"{img.stem}.txt").write_text(
                        "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n",
                        encoding="utf-8",
                    )
                    n += 1
                from cs2_vision_access.training.prepare_lib import PrepareResult

                return PrepareResult(labeled_frames=n, write_root=str(data), notes=[])

            ensure = MagicMock(return_value=assets)
            prepare = MagicMock(side_effect=_prepare)

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
                    coco,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.bootstrap.ensure_edgesam_assets",
                    ensure,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.bootstrap.run_cs2_sam_prepare",
                    prepare,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=True,
                    min_label_ratio=0.05,
                    use_edgesam=True,
                    download_edgesam=False,
                    edgesam_artifacts_dir=art,
                    resume=False,
                    use_cs2_10k=False,
                    allow_leaky_val=True,
                )

            ensure.assert_called()
            prepare.assert_called()
            coco.assert_not_called()
            self.assertGreater(report.bootstrap_labels_written, 0)
            self.assertTrue(
                any("edgesam prepare labeled=" in n for n in report.notes),
                report.notes,
            )

    def test_use_edgesam_failure_falls_back_to_coco(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp) / "data"
            self._seed_unlabeled(data, n=4)

            def _coco(data_dir, **k):
                labels = Path(data_dir) / "labels"
                labels.mkdir(exist_ok=True)
                written = 0
                for img in (Path(data_dir) / "images").glob("*.jpg"):
                    (labels / f"{img.stem}.txt").write_text(
                        "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n",
                        encoding="utf-8",
                    )
                    written += 1
                return written

            coco = MagicMock(side_effect=_coco)
            ensure = MagicMock(side_effect=FileNotFoundError("no edgesam assets"))

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
                    coco,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.bootstrap.ensure_edgesam_assets",
                    ensure,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=True,
                    min_label_ratio=0.05,
                    use_edgesam=True,
                    edgesam_coco_fallback=True,
                    resume=False,
                    use_cs2_10k=False,
                    allow_leaky_val=True,
                )

            ensure.assert_called()
            coco.assert_called()
            self.assertTrue(
                any("edgesam" in n and "failed" in n for n in report.notes),
                report.notes,
            )
            self.assertTrue(
                any("coco person bootstrap" in n for n in report.notes),
                report.notes,
            )

    def test_use_edgesam_false_coco_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp) / "data"
            self._seed_unlabeled(data, n=4)

            def _coco(data_dir, **k):
                labels = Path(data_dir) / "labels"
                labels.mkdir(exist_ok=True)
                written = 0
                for img in (Path(data_dir) / "images").glob("*.jpg"):
                    (labels / f"{img.stem}.txt").write_text(
                        "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n",
                        encoding="utf-8",
                    )
                    written += 1
                return written

            coco = MagicMock(side_effect=_coco)
            ensure = MagicMock()

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
                    coco,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.bootstrap.ensure_edgesam_assets",
                    ensure,
                ),
            ):
                report = run_autonomous_loop(
                    dataset_zip=None,
                    data_dir=data,
                    iterations=1,
                    epochs_per_iter=1,
                    batch=2,
                    install_deps=False,
                    bootstrap_if_needed=True,
                    min_label_ratio=0.05,
                    use_edgesam=False,
                    resume=False,
                    use_cs2_10k=False,
                    allow_leaky_val=True,
                )

            ensure.assert_not_called()
            coco.assert_called()
            self.assertTrue(
                any("coco person bootstrap" in n for n in report.notes),
                report.notes,
            )


if __name__ == "__main__":
    unittest.main()
