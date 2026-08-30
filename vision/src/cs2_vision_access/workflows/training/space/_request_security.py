"""Server-side admission checks for Space training requests."""

from __future__ import annotations

import importlib
import math
import zipfile
from collections.abc import Collection, Mapping
from pathlib import Path
from typing import Any


def canonicalize_space_dataset(data_dir: str | Path) -> Path:
    """Create the fixed manifest required by the untrusted Space upload path."""
    try:
        module = importlib.import_module("cs2_vision_access.workflows.training.cloud.dataset")
    except ImportError:
        try:
            module = importlib.import_module("cloud.dataset")
        except ImportError as exc:
            raise RuntimeError(
                "Training helpers unavailable: cloud.dataset is required to validate "
                "uploaded datasets."
            ) from exc
    creator = getattr(module, "create_space_dataset_yaml", None)
    if not callable(creator):
        raise RuntimeError("cloud.dataset does not expose the required manifest validator.")
    created = creator(data_dir)
    if isinstance(created, Path):
        return created
    raise RuntimeError("cloud.dataset returned an invalid dataset manifest path.")


def confined_dataset_root(data_dir: str | Path, extract_dir: Path) -> Path:
    """Return a real dataset root confined to the private extraction directory."""
    extraction_root = extract_dir.resolve(strict=True)
    candidate = Path(data_dir)
    if candidate.is_symlink():
        raise ValueError("dataset root must not be a symbolic link")
    dataset_root = candidate.resolve(strict=True)
    try:
        dataset_root.relative_to(extraction_root)
    except ValueError as exc:
        raise ValueError("dataset root escaped the private extraction directory") from exc
    if not dataset_root.is_dir():
        raise ValueError("dataset root is not a directory")

    for directory_name in ("images", "labels"):
        directory = dataset_root / directory_name
        if directory.is_symlink():
            raise ValueError(f"dataset {directory_name}/ must not be a symbolic link")
        resolved = directory.resolve(strict=True)
        try:
            resolved.relative_to(dataset_root)
        except ValueError as exc:
            raise ValueError(f"dataset {directory_name}/ escaped the dataset root") from exc
        if not resolved.is_dir():
            raise ValueError(f"dataset {directory_name}/ is not a directory")

    manifest = dataset_root / "dataset.yaml"
    if manifest.is_symlink():
        raise ValueError("dataset.yaml must not be a symbolic link")
    return dataset_root


def required_space_auth(environment: Mapping[str, str]) -> tuple[str, str]:
    """Return required private-Space credentials without exposing their values."""
    username = environment.get("CS2_SPACE_AUTH_USER", "").strip()
    password = environment.get("CS2_SPACE_AUTH_PASSWORD", "").strip()
    if not username or not password:
        raise RuntimeError(
            "CS2_SPACE_AUTH_USER and CS2_SPACE_AUTH_PASSWORD must be set before launch."
        )
    return username, password


def resolve_upload_path(dataset_zip: Any) -> str | None:
    """Normalize Gradio's path-like upload values."""
    if dataset_zip is None:
        return None
    if isinstance(dataset_zip, (str, Path)):
        path = str(dataset_zip).strip()
        return path or None
    name = getattr(dataset_zip, "name", None)
    if name:
        path = str(name).strip()
        return path or None
    return None


def validate_uploaded_zip(
    zip_path: Path,
    *,
    max_upload_bytes: int,
    max_members: int,
    max_member_bytes: int,
    max_total_bytes: int,
    max_compression_ratio: float,
) -> str | None:
    """Return a user-safe error when an upload exceeds admission limits."""
    try:
        if zip_path.stat().st_size > max_upload_bytes:
            return f"Error: dataset zip exceeds the upload limit ({max_upload_bytes} bytes)."
        with zipfile.ZipFile(zip_path, "r") as archive:
            members = archive.infolist()
    except (OSError, zipfile.BadZipFile):
        return "Error: uploaded file is not a readable zip archive."
    if len(members) > max_members:
        return f"Error: dataset zip has too many members (maximum {max_members})."

    total_bytes = 0
    for info in members:
        if info.file_size > max_member_bytes:
            return (
                "Error: dataset zip contains a member that exceeds the expanded-size limit "
                f"({max_member_bytes} bytes)."
            )
        total_bytes += info.file_size
        if total_bytes > max_total_bytes:
            return (
                "Error: dataset zip exceeds the total expanded-size limit "
                f"({max_total_bytes} bytes)."
            )
        if info.file_size and (
            info.compress_size == 0 or info.file_size / info.compress_size > max_compression_ratio
        ):
            return (
                "Error: dataset zip contains a member above the compression-ratio limit "
                f"({max_compression_ratio:g}:1)."
            )
    return None


def integer_in_range(value: Any, minimum: int, maximum: int) -> int | None:
    """Coerce a finite integral value only when it is within the bound."""
    if isinstance(value, bool):
        return None
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(numeric) or not numeric.is_integer():
        return None
    result = int(numeric)
    return result if minimum <= result <= maximum else None


def validate_training_controls(
    base_model: Any,
    epochs: Any,
    batch: Any,
    imgsz: Any,
    lr0: Any,
    patience: Any,
    *,
    base_models: Collection[str],
    image_sizes: Collection[int],
) -> tuple[str, int, int, int, float, int] | str:
    """Validate direct requests independently of client-side widgets."""
    if not isinstance(base_model, str) or base_model not in base_models:
        return "Error: unsupported base model."
    checked_epochs = integer_in_range(epochs, 10, 500)
    checked_batch = integer_in_range(batch, 2, 64)
    checked_imgsz = integer_in_range(imgsz, min(image_sizes), max(image_sizes))
    checked_patience = integer_in_range(patience, 10, 200)
    try:
        checked_lr0 = float(lr0)
    except (TypeError, ValueError):
        checked_lr0 = float("nan")
    if (
        checked_epochs is None
        or checked_batch is None
        or checked_imgsz is None
        or checked_imgsz not in image_sizes
        or checked_patience is None
        or not math.isfinite(checked_lr0)
        or not 0.0001 <= checked_lr0 <= 0.01
    ):
        return "Error: training controls are outside the supported server-side bounds."
    return base_model, checked_epochs, checked_batch, checked_imgsz, checked_lr0, checked_patience
