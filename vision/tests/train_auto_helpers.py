"""Shared fixtures/helpers for train-auto tests."""

from __future__ import annotations

import json
import zipfile
from pathlib import Path

from cs2_vision_access.training.local import TrainingSummary

POLYGON = "0 0.1 0.1 0.8 0.1 0.5 0.9\n"


def _write_split_dataset(root: Path, *, class_names: dict[int, str] | None = None) -> Path:
    """Minimal valid YOLO-seg split tree (train + val)."""
    names = class_names or {0: "ct", 1: "t"}
    for split in ("train", "val"):
        (root / "images" / split).mkdir(parents=True, exist_ok=True)
        (root / "labels" / split).mkdir(parents=True, exist_ok=True)
    (root / "images" / "train" / "a.png").write_bytes(b"image")
    (root / "labels" / "train" / "a.txt").write_text(POLYGON, encoding="utf-8")
    (root / "images" / "val" / "b.png").write_bytes(b"image")
    (root / "labels" / "val" / "b.txt").write_text("", encoding="utf-8")
    names_block = "\n".join(f"  {k}: {v}" for k, v in sorted(names.items()))
    (root / "dataset.yaml").write_text(
        f"path: {root.resolve().as_posix()}\n"
        f"train: images/train\n"
        f"val: images/val\n"
        f"nc: {len(names)}\n"
        f"names:\n{names_block}\n",
        encoding="utf-8",
    )
    return root


def _write_flat_dataset(root: Path) -> Path:
    (root / "images").mkdir(parents=True, exist_ok=True)
    (root / "labels").mkdir(parents=True, exist_ok=True)
    (root / "images" / "frame.jpg").write_bytes(b"fake-image")
    (root / "labels" / "frame.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
    (root / "dataset.yaml").write_text(
        f"path: {root.resolve().as_posix()}\n"
        "train: images\n"
        "val: images\n"
        "nc: 1\n"
        "names:\n  0: player\n",
        encoding="utf-8",
    )
    return root


def _write_flat_zip(zip_path: Path) -> None:
    with zipfile.ZipFile(zip_path, "w") as zf:
        zf.writestr("images/frame.jpg", b"fake-image")
        zf.writestr("labels/frame.txt", "0 0.5 0.5 0.1 0.1\n")
        zf.writestr(
            "dataset.yaml",
            "path: .\ntrain: images\nval: images\nnc: 1\nnames:\n  0: player\n",
        )


def _mock_local_train(
    *,
    out_dir: Path,
    **kwargs: object,
) -> TrainingSummary:
    out_dir.mkdir(parents=True, exist_ok=True)
    weights = out_dir / "weights"
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
    # Optional results.csv so progress/metrics path can be exercised.
    (out_dir / "results.csv").write_text(
        "epoch,metrics/mAP50(B),metrics/mAP50-95(B)\n1,0.55,0.30\n",
        encoding="utf-8",
    )
    return TrainingSummary(
        run_directory=str(out_dir),
        best_checkpoint=str(weights / "best.pt"),
        onnx_model=str(onnx),
        manifest=str(manifest),
    )


def _mock_cloud_train(data_dir: Path, **kwargs: object) -> Path:
    data_dir = Path(data_dir)
    onnx = data_dir / "cs2-yolo11n-seg.onnx"
    onnx.write_bytes(b"fake-cloud-onnx")
    # If project/run_name provided, drop a results.csv for progress hooks.
    project = kwargs.get("project")
    run_name = kwargs.get("run_name")
    if project is not None and run_name is not None:
        run_dir = Path(str(project)) / str(run_name)
        run_dir.mkdir(parents=True, exist_ok=True)
        (run_dir / "results.csv").write_text(
            "epoch,metrics/mAP50(B)\n1,0.4\n",
            encoding="utf-8",
        )
    return onnx
