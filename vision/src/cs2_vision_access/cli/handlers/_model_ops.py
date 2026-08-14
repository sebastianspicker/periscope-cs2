"""Shared model operations — ONNX export, manifest creation, hashing, COCO classes.

These were extracted from ``download_model.py`` so that ``setup.py`` (and
other callers) can import them without depending on the download CLI handler.
"""

from __future__ import annotations

from pathlib import Path

from cs2_vision_access.model_manifest import create_manifest as write_model_manifest
from cs2_vision_access.model_manifest import sha256_file as _canonical_sha256_file


class ModelOperationError(RuntimeError):
    """Model export or manifest creation failed."""


def export_onnx(pt_path: Path, onnx_path: Path, *, image_size: int, device: str) -> None:
    """Load a .pt checkpoint and export to ONNX."""
    try:
        from ultralytics import YOLO
    except ImportError as error:
        raise ModelOperationError(
            "Ultralytics is required for ONNX export; install project dependencies"
        ) from error

    try:
        model = YOLO(str(pt_path))
        exported_path = model.export(
            format="onnx",
            imgsz=image_size,
            dynamic=False,
            simplify=False,
            device=device,
        )
        exported = Path(exported_path)
        if exported.resolve() != onnx_path.resolve():
            import shutil

            shutil.move(str(exported), str(onnx_path))
    except Exception as error:
        raise ModelOperationError(f"ONNX export failed for {pt_path.name}: {error}") from error


def create_manifest(
    onnx_path: Path,
    manifest_path: Path,
    *,
    model_name: str,
    model_info: dict[str, str],
    image_size: int,
    origin: str,
    license_name: str,
    classes: list[str] | None,
) -> None:
    """Write a SHA-256 manifest JSON for the exported ONNX model.

    CLI-friendly wrapper around
    :func:`cs2_vision_access.model_manifest.create_manifest`. Preserves origin
    enrichment (``download-model {model_name}``) used by download-model / setup.
    ``model_info`` and ``image_size`` are accepted for call-site compatibility.
    """
    del model_info, image_size  # API compatibility only

    if classes:
        class_dict = {i: name for i, name in enumerate(classes)}
    else:
        class_dict = {int(key): name for key, name in coco80_classes().items()}

    write_model_manifest(
        onnx_path,
        manifest_path,
        classes=class_dict,
        origin=f"{origin}; download-model {model_name}",
        license_name=license_name,
        overwrite=True,
    )


def sha256_file(path: Path) -> str:
    """Compute the SHA-256 hex digest of a file."""
    return _canonical_sha256_file(path)


def coco80_classes() -> dict[str, str]:
    """Return the standard COCO 80-class mapping as a str→str dict.

    Source of truth: ``cs2_vision_access.config.data.coco80.json``.
    """
    from cs2_vision_access.config.data import coco80_classes as _load

    return dict(_load())
