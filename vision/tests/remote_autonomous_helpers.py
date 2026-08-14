"""Shared fixtures/helpers for remote autonomous loop tests."""

from __future__ import annotations

from pathlib import Path

import numpy as np


class AutonomousLoopFakesMixin:
    """Reusable train/manifest/package/self-train fakes for autonomous loop tests."""

    def _fake_train(self, data_dir, *a, **k):
        data_dir = Path(data_dir)
        onnx = data_dir / "cs2-yolo11n-seg.onnx"
        onnx.write_bytes(b"onnx")
        project = Path(k.get("project") or (data_dir / "runs"))
        name = k.get("run_name") or "train"
        run = Path(project) / name
        run.mkdir(parents=True, exist_ok=True)
        # Vary mAP by iter so best-iter tracking is exercisable
        map_val = 0.5 if "iter_1" in name else 0.7
        (run / "results.csv").write_text(
            f"epoch,train/box_loss,metrics/mAP50(B)\n1,1.0,{map_val - 0.1}\n2,0.8,{map_val}\n",
            encoding="utf-8",
        )
        return onnx

    def _fake_manifest(self, onnx_path, data_dir, classes=None, **k):
        p = Path(data_dir) / "cs2-yolo11n-seg.model.json"
        p.write_text("{}", encoding="utf-8")
        # Also snapshot-shaped iter manifests when callers snapshot later.
        return p

    def _fake_package(self, onnx_path, manifest_path, output_zip, **k):
        out = Path(output_zip)
        out.write_bytes(b"PK\x03\x04fake")
        # Soft-fail path should receive held_out_split / report when present.
        extras = k.get("extra_paths") or []
        {Path(p).name for p in extras}
        # Not strictly required every call, but when present they must be real files.
        for p in extras:
            self.assertTrue(Path(p).is_file(), p)
        return out

    def _fake_self_train(self, *a, **k):
        # Write into the labels_dir passed as arg 1 (train split when held-out).
        labels_dir = Path(a[1])
        lab = labels_dir / "frame_00000001.txt"
        lab.write_text(
            "# pseudo\n0 0.2 0.2 0.8 0.2 0.8 0.8 0.2 0.8\n",
            encoding="utf-8",
        )
        from cs2_vision_access.training.self_train import SelfTrainReport

        # overwrite_pseudo path must be accepted by the loop.
        self.assertEqual(k.get("write_policy"), "overwrite_pseudo")
        allowed = k.get("allowed_class_ids")
        self.assertIsNotNone(allowed, "self-train must receive allowed_class_ids")
        self.assertIn(0, set(allowed))
        return SelfTrainReport(
            images_scanned=5,
            already_labeled=1,
            unlabeled_scanned=2,
            accepted=1,
            rejected_low_conf=0,
            rejected_empty=1,
            conf_threshold=0.45,
            labels_written=("frame_00000001.txt",),
        )

    def _seed_dataset(self, data: Path, n: int = 5, labeled: int = 3) -> None:
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
        for i in range(labeled):
            (labels / f"frame_{i:08d}.txt").write_text(
                "0 0.1 0.1 0.9 0.1 0.9 0.9 0.1 0.9\n", encoding="utf-8"
            )
