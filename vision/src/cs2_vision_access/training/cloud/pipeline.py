"""Full cloud training pipeline: extract → train → export → manifest → package."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path

from .constants import (
    DEFAULT_BASE_MODEL,
    DEFAULT_BATCH,
    DEFAULT_CLASSES,
    DEFAULT_EPOCHS,
    DEFAULT_IMGSZ,
    DEFAULT_LR0,
    DEFAULT_PATIENCE,
    OUTPUT_MANIFEST_NAME,
    OUTPUT_ONNX_FP16_NAME,
)
from .dataset import _classes_to_yaml_names, extract_dataset, package_outputs
from .deps import install_dependencies
from .fallbacks import sha256_file, write_model_manifest
from .train import train


def create_manifest(
    onnx_path: str | Path,
    data_dir: str | Path,
    classes: Mapping[int, str] | Mapping[str, str] | None = None,
    *,
    origin: str = "Fine-tuned on CS2 pseudo-labels",
    license_name: str = "AGPL-3.0-only",
) -> Path:
    """Create a model manifest JSON next to the ONNX file.

    Thin notebook-friendly wrapper around
    :func:`cs2_vision_access.model_manifest.create_manifest`.

    Args:
        onnx_path: Path to the ONNX weights file.
        data_dir: Directory where the manifest JSON is written.
        classes: Optional class map override. Defaults to :data:`DEFAULT_CLASSES`.
        origin: Free-text provenance for the manifest ``origin`` field.
        license_name: SPDX-style license string for the manifest ``license`` field.
    """
    onnx_path = Path(onnx_path)
    if not onnx_path.is_file():
        raise FileNotFoundError(f"ONNX not found for manifest: {onnx_path}")

    class_map = _classes_to_yaml_names(classes) if classes is not None else dict(DEFAULT_CLASSES)
    data_dir = Path(data_dir)
    data_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = data_dir / OUTPUT_MANIFEST_NAME
    result = write_model_manifest(
        onnx_path,
        manifest_path,
        classes=class_map,
        origin=origin,
        license_name=license_name,
        overwrite=True,
    )
    digest = sha256_file(onnx_path)
    print(f"✓ Manifest: {result}")
    print(f"  SHA-256: {digest}")
    return result


def run_pipeline(
    dataset_zip: str | Path,
    *,
    base_model: str = DEFAULT_BASE_MODEL,
    epochs: int = DEFAULT_EPOCHS,
    batch: int = DEFAULT_BATCH,
    imgsz: int = DEFAULT_IMGSZ,
    lr0: float = DEFAULT_LR0,
    patience: int = DEFAULT_PATIENCE,
    device: str | None = None,
    output_dir: str = "cs2_data",
    classes: Mapping[int, str] | Mapping[str, str] | None = None,
    origin: str = "Fine-tuned on CS2 pseudo-labels",
    resume: bool = False,
    verbose: bool = True,
    progress_callback: object | None = None,
    project: str | Path | None = None,
    run_name: str | None = None,
    plots: bool = True,
) -> tuple[Path, Path]:
    """Run the complete training pipeline: extract → train → export → manifest.

    When ONNX and manifest both exist after training, also writes a download
    zip next to the dataset directory via :func:`package_outputs`
    (``<data_dir>/cs2-yolo11n-seg-bundle.zip``).

    ``project``, ``run_name``, and ``plots`` are forwarded to :func:`train`
    (stable Ultralytics run dir under ``data_dir/runs/<run_name>`` by default).

    Returns:
        (onnx_path, manifest_path) — FP32 ONNX and manifest. FP16 sibling is
        written beside the ONNX when conversion succeeds.
    """
    install_dependencies()
    data_dir = extract_dataset(dataset_zip, output_dir)
    onnx_path = train(
        data_dir,
        base_model,
        epochs,
        batch,
        imgsz,
        lr0,
        patience,
        device,
        classes=classes,
        resume=resume,
        verbose=verbose,
        progress_callback=progress_callback,
        project=project,
        run_name=run_name,
        plots=plots,
    )
    manifest_path = create_manifest(onnx_path, data_dir, classes=classes, origin=origin)

    # Always package for notebook download when artifacts exist.
    if Path(onnx_path).is_file() and Path(manifest_path).is_file():
        bundle_zip = Path(data_dir) / "cs2-yolo11n-seg-bundle.zip"
        try:
            package_outputs(onnx_path, manifest_path, bundle_zip)
        except Exception as exc:  # noqa: BLE001 — packaging must not fail the run
            print(f"(package_outputs skipped: {exc})")

    print(f"\n{'=' * 60}")
    print("Training complete!")
    print(f"  ONNX:     {onnx_path}")
    print(f"  Manifest: {manifest_path}")
    fp16 = Path(onnx_path).with_name(OUTPUT_ONNX_FP16_NAME)
    if not fp16.is_file():
        fp16 = Path(onnx_path).with_name(f"{Path(onnx_path).stem}-fp16.onnx")
    if fp16.is_file():
        print(f"  FP16:     {fp16}")
    bundle = Path(data_dir) / "cs2-yolo11n-seg-bundle.zip"
    if bundle.is_file():
        print(f"  Bundle:   {bundle}")
    print(f"{'=' * 60}")

    return onnx_path, manifest_path


# ---------------------------------------------------------------------------
# CLI entry point (for testing)
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="CS2 YOLO training pipeline")
    parser.add_argument("dataset_zip", help="Path to dataset zip file")
    parser.add_argument("--base-model", default=DEFAULT_BASE_MODEL)
    parser.add_argument("--epochs", type=int, default=DEFAULT_EPOCHS)
    parser.add_argument("--batch", type=int, default=DEFAULT_BATCH)
    parser.add_argument("--imgsz", type=int, default=DEFAULT_IMGSZ)
    parser.add_argument("--lr0", type=float, default=DEFAULT_LR0)
    parser.add_argument("--patience", type=int, default=DEFAULT_PATIENCE)
    parser.add_argument("--device", default=None)
    parser.add_argument("--output-dir", default="cs2_data")
    parser.add_argument(
        "--origin",
        default="Fine-tuned on CS2 pseudo-labels",
        help="Manifest origin string",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Resume Ultralytics training from the last checkpoint",
    )
    args = parser.parse_args()

    run_pipeline(
        args.dataset_zip,
        base_model=args.base_model,
        epochs=args.epochs,
        batch=args.batch,
        imgsz=args.imgsz,
        lr0=args.lr0,
        patience=args.patience,
        device=args.device,
        output_dir=args.output_dir,
        origin=args.origin,
        resume=args.resume,
    )
