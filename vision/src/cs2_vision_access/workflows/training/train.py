#!/usr/bin/env python3
"""Fine-tune YOLO11n-seg on CS2 pseudo-labeled data from prepare_dataset.py.

Usage::

    uv run python -m cs2_vision_access.workflows.training.train --data data/cs2_train/dataset.yaml

    # With custom settings (416px = 2× faster, minimal quality loss):
    uv run python -m cs2_vision_access.workflows.training.train ^
        --data data/cs2_train/dataset.yaml ^
        --model yolo11n-seg.pt ^
        --epochs 150 ^
        --batch 32 ^
        --device cpu ^
        --imgsz 416

This produces a fine-tuned ONNX model and manifest at:
    artifacts/cs2-yolo11n-seg.onnx
    artifacts/cs2-yolo11n-seg.model.json

Defaults (epochs, batch, imgsz, lr0, patience, classes) come from
:mod:`cs2_vision_access.workflows.training.contracts` profile ``self_train`` /
:data:`VOMBIT_CLASSES`.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Mapping
from pathlib import Path

# Ensure the project src is on sys.path.
_PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))


class LeakyValidationError(ValueError):
    """Raised when train and val paths resolve to the same data (leaky val)."""


def _normalize_split_path(
    value: str,
    *,
    dataset_yaml_path: str | Path | None = None,
) -> str:
    """Normalise a train/val path string; resolve relatives against yaml parent."""
    text = str(value).strip().replace("\\", "/").rstrip("/")
    if not text:
        return ""
    path = Path(text)
    if dataset_yaml_path is not None and not path.is_absolute():
        resolved = (Path(dataset_yaml_path).resolve().parent / path).resolve()
        return str(resolved).replace("\\", "/").rstrip("/")
    if path.is_absolute():
        try:
            return str(path.resolve()).replace("\\", "/").rstrip("/")
        except OSError:
            return text
    return text


def train_val_paths_are_leaky(
    train_rel: str | None,
    val_rel: str | None,
    *,
    dataset_yaml_path: str | Path | None = None,
) -> bool:
    """Return True when train and val refer to the same path.

    Pure helper used by CLI and tests. Compares normalised path strings (strip,
    collapse ``\\`` → ``/``, strip trailing ``/``). When ``dataset_yaml_path`` is
    set, relative train/val values are resolved against that yaml's parent so
    equivalent absolute/relative forms still detect a leak. Empty / missing
    values are not treated as leaky.
    """
    if train_rel is None or val_rel is None:
        return False
    train_s = _normalize_split_path(str(train_rel), dataset_yaml_path=dataset_yaml_path)
    val_s = _normalize_split_path(str(val_rel), dataset_yaml_path=dataset_yaml_path)
    if not train_s or not val_s:
        return False
    return train_s == val_s


def parse_dataset_yaml_train_val(text: str) -> tuple[str | None, str | None]:
    """Extract train: and val: path values from dataset.yaml text.

    Minimal parser — avoids requiring PyYAML for the leaky-val gate.
    """
    train_m = re.search(r"(?m)^train:\s*(.+?)\s*$", text)
    val_m = re.search(r"(?m)^val:\s*(.+?)\s*$", text)
    train = train_m.group(1).strip().strip("\"'") if train_m else None
    val = val_m.group(1).strip().strip("\"'") if val_m else None
    return train, val


def check_leaky_val(
    dataset_yaml: str | Path,
    *,
    allow_leaky_val: bool = False,
) -> None:
    """Refuse train==val unless ``allow_leaky_val`` is True.

    Raises:
        LeakyValidationError: when train and val paths match and not allowed.
        FileNotFoundError: when the yaml path is missing.
    """
    path = Path(dataset_yaml)
    if not path.is_file():
        raise FileNotFoundError(f"dataset.yaml not found: {path}")
    text = path.read_text(encoding="utf-8")
    train_rel, val_rel = parse_dataset_yaml_train_val(text)
    if train_val_paths_are_leaky(train_rel, val_rel, dataset_yaml_path=path):
        if allow_leaky_val:
            print(
                "WARNING: train and val paths are identical "
                f"({train_rel!r}); continuing because --allow-leaky-val was set."
            )
            return
        raise LeakyValidationError(
            f"dataset.yaml has the same path for train and val ({train_rel!r}). "
            "This leaks validation into training metrics. "
            "Use a held-out val split, or pass --allow-leaky-val for small "
            "pseudo-label / smoke runs."
        )


def _build_parser() -> argparse.ArgumentParser:
    from cs2_vision_access.workflows.training.contracts import PROFILES, VOMBIT_CLASSES

    # Align defaults with cloud_t4 / self_train profile (SSOT in contracts).
    profile = PROFILES.get("self_train") or PROFILES["cloud_t4"]
    class_names = ", ".join(VOMBIT_CLASSES[i] for i in sorted(VOMBIT_CLASSES))
    p = argparse.ArgumentParser(
        description="Fine-tune YOLO11n-seg on CS2 player segmentation data",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument(
        "--data",
        required=True,
        help="Path to dataset.yaml (generated by prepare_dataset.py)",
    )
    p.add_argument(
        "--model",
        default=profile.base_model,
        help=f"Starting checkpoint (default: {profile.base_model})",
    )
    p.add_argument(
        "--epochs",
        type=int,
        default=profile.epochs,
        help=f"Number of training epochs (default: {profile.epochs})",
    )
    p.add_argument(
        "--batch",
        type=int,
        default=profile.batch,
        help=f"Batch size (default: {profile.batch}; lower for CPU)",
    )
    p.add_argument(
        "--device",
        default="cpu",
        help="Training device: 'cpu', 'cuda:0', 'mps' (default: cpu)",
    )
    p.add_argument(
        "--imgsz",
        type=int,
        default=profile.image_size,
        help=(
            f"Training image size (default: {profile.image_size} — 2× faster than 640, "
            "minimal quality loss)"
        ),
    )
    p.add_argument(
        "--lr0",
        type=float,
        default=profile.lr0,
        help=f"Initial learning rate (default: {profile.lr0})",
    )
    p.add_argument(
        "--patience",
        type=int,
        default=profile.patience,
        help=f"Early stopping patience (default: {profile.patience})",
    )
    p.add_argument(
        "--output",
        default=None,
        help="Output directory for trained model (default: runs/train)",
    )
    p.add_argument(
        "--workers",
        type=int,
        default=0,
        help="Data loader workers (default: 0, set higher for GPU)",
    )
    p.add_argument(
        "--resume",
        action="store_true",
        help="Resume from last checkpoint",
    )
    p.add_argument(
        "--allow-leaky-val",
        action="store_true",
        help=(
            "Allow train and val to point at the same path in dataset.yaml "
            "(discouraged; metrics will be optimistic)"
        ),
    )
    p.add_argument(
        "--classes",
        nargs="*",
        default=None,
        metavar="NAME",
        help=(f"Class names for the exported manifest in id order (default: {class_names})"),
    )
    return p


def _resolve_class_map(
    class_names: list[str] | None,
    default_classes: Mapping[int, str],
) -> dict[int, str]:
    """Build id→name map from CLI names or contracts VOMBIT_CLASSES."""
    if class_names:
        return {i: name for i, name in enumerate(class_names)}
    return {int(k): str(v) for k, v in default_classes.items()}


def main() -> None:
    from cs2_vision_access.workflows.training.contracts import VOMBIT_CLASSES
    from cs2_vision_access.workflows.training.train_core import build_train_kwargs

    parser = _build_parser()
    args = parser.parse_args()

    data_path = Path(args.data)
    if not data_path.is_file():
        parser.error(f"dataset.yaml not found: {data_path}")

    try:
        check_leaky_val(data_path, allow_leaky_val=args.allow_leaky_val)
    except LeakyValidationError as error:
        parser.error(str(error))

    print("=" * 60)
    print("CS2 Segmentation Self-Training")
    print("=" * 60)
    print(f"  Data:          {data_path}")
    print(f"  Base model:    {args.model}")
    print(f"  Epochs:        {args.epochs}")
    print(f"  Batch:         {args.batch}")
    print(f"  Device:        {args.device}")
    print(f"  Image size:    {args.imgsz}")
    print(f"  Learning rate: {args.lr0}")
    print("=" * 60)

    # ------------------------------------------------------------------
    # Load and train
    # ------------------------------------------------------------------
    try:
        from ultralytics import YOLO
    except ImportError as error:
        raise RuntimeError(
            "ultralytics is required for training; run 'uv sync' or 'pip install ultralytics'"
        ) from error

    model = YOLO(args.model)

    # Shared kwargs assembly; CLI-only Ultralytics extras merged below.
    train_kwargs = build_train_kwargs(
        data=str(data_path),
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device,
        project=args.output if args.output is not None else "runs/train",
        name="cs2-seg-finetune",
        exist_ok=True,
        plots=True,
        resume=args.resume,
        lr0=args.lr0,
        patience=args.patience,
        workers=args.workers,
        amp=True,  # automatic mixed precision if available
        seed=42,
        verbose=True,
        val=True,
    )
    train_kwargs.update(
        pretrained=True,
        optimizer="auto",
        fraction=1.0,
        save=True,
        save_period=25,  # checkpoint every 25 epochs
    )

    print("\nStarting training...")
    results = model.train(**train_kwargs)

    # Find the best model path.
    best_pt = Path(results.save_dir) / "weights" / "best.pt"
    if not best_pt.is_file():
        raise RuntimeError(f"training did not produce best.pt at {best_pt}")

    # ------------------------------------------------------------------
    # Export to ONNX
    # ------------------------------------------------------------------
    print(f"\nExporting best model ({best_pt}) to ONNX...")

    model = YOLO(str(best_pt))  # reload best
    model.export(
        format="onnx",
        imgsz=args.imgsz,
        simplify=True,
        dynamic=False,
    )

    # model.export() saves next to the .pt with .onnx extension.
    exported_onnx = best_pt.with_suffix(".onnx")
    if not exported_onnx.is_file():
        raise RuntimeError(f"ONNX export failed: {exported_onnx} not found")

    # ------------------------------------------------------------------
    # Copy to artifacts with canonical name
    # ------------------------------------------------------------------
    artifacts_dir = _PROJECT_ROOT / "artifacts"
    artifacts_dir.mkdir(exist_ok=True)

    final_onnx = artifacts_dir / "cs2-yolo11n-seg.onnx"
    import shutil

    shutil.copy2(str(exported_onnx), str(final_onnx))

    # ------------------------------------------------------------------
    # Create manifest (canonical writer + contracts VOMBIT class map)
    # ------------------------------------------------------------------
    from cs2_vision_access.application.model_assets.manifest import create_manifest, sha256_file

    class_map = _resolve_class_map(args.classes, VOMBIT_CLASSES)
    manifest_path = artifacts_dir / "cs2-yolo11n-seg.model.json"
    create_manifest(
        final_onnx,
        manifest_path,
        classes=dict(class_map),
        origin="Self-trained: YOLO11n-seg fine-tuned on CS2 pseudo-labels",
        license_name="AGPL-3.0-only",
        overwrite=True,
    )
    sha256 = sha256_file(final_onnx)

    print(f"\n{'=' * 60}")
    print("Training complete!")
    print(f"  ONNX model:  {final_onnx}  ({final_onnx.stat().st_size / 1e6:.1f} MB)")
    print(f"  Manifest:    {manifest_path}")
    print(f"  SHA-256:     {sha256}")
    print("\nRun inference:")
    print(f"  cs2-vision live --model {final_onnx} --manifest {manifest_path} --class-name ct t")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
