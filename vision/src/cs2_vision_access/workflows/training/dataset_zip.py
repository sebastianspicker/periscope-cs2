"""Pure helpers for safe YOLO dataset zip extraction and root resolution.

Shared by cloud training notebooks and the monorepo package. Kept free of
training / notebook side effects so it can be imported without Ultralytics.
"""

from __future__ import annotations

import re
import shutil
import zipfile
from dataclasses import dataclass
from pathlib import Path

# Align with dataset.types; soft-import so this module still works when vendored alone.
try:
    from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS
except ImportError:
    IMAGE_EXTENSIONS = frozenset({".jpg", ".jpeg", ".png", ".bmp", ".webp"})

# Stable preference order for stem → image lookup (subset of IMAGE_EXTENSIONS).
_PREFERRED_IMAGE_EXTS: tuple[str, ...] = (".jpg", ".jpeg", ".png", ".bmp", ".webp")


@dataclass(frozen=True)
class ZipExtractionLimits:
    """Metadata limits applied before any zip member is extracted."""

    max_members: int = 10_000
    max_member_uncompressed_bytes: int = 128 * 1024 * 1024
    max_total_uncompressed_bytes: int = 512 * 1024 * 1024
    max_compression_ratio: float = 100.0


DEFAULT_ZIP_EXTRACTION_LIMITS = ZipExtractionLimits()


def is_unsafe_zip_member(name: str) -> bool:
    """Return True if a zip member path is absolute or uses path traversal."""
    if not name or name.endswith("/"):
        # Directory entries alone are fine; still check their components below.
        pass

    # Zip uses forward slashes; also reject backslashes and drive letters.
    normalised = name.replace("\\", "/")
    if normalised.startswith("/") or re.match(r"^[A-Za-z]:", normalised):
        return True

    parts = Path(normalised).parts
    return bool(any(part == ".." for part in parts))


def validate_zip_members(
    zf: zipfile.ZipFile,
    limits: ZipExtractionLimits = DEFAULT_ZIP_EXTRACTION_LIMITS,
) -> None:
    """Reject unsafe or excessively expanded members before extraction begins."""
    members = zf.infolist()
    if len(members) > limits.max_members:
        raise ValueError(f"Dataset zip has too many members (maximum {limits.max_members}).")

    total_uncompressed_bytes = 0
    for info in members:
        name = info.filename
        if is_unsafe_zip_member(name):
            raise ValueError(
                f"Refusing to extract unsafe zip member (path traversal or absolute path): {name!r}"
            )
        if info.file_size > limits.max_member_uncompressed_bytes:
            raise ValueError(
                "Dataset zip member exceeds the expanded-size limit "
                f"({limits.max_member_uncompressed_bytes} bytes): {name!r}"
            )
        total_uncompressed_bytes += info.file_size
        if total_uncompressed_bytes > limits.max_total_uncompressed_bytes:
            raise ValueError(
                "Dataset zip exceeds the total expanded-size limit "
                f"({limits.max_total_uncompressed_bytes} bytes)."
            )
        if info.file_size and (
            info.compress_size == 0
            or info.file_size / info.compress_size > limits.max_compression_ratio
        ):
            raise ValueError(
                "Dataset zip member exceeds the compression-ratio limit "
                f"({limits.max_compression_ratio:g}:1): {name!r}"
            )


def safe_extract_zip(
    zf: zipfile.ZipFile,
    dest: Path,
    limits: ZipExtractionLimits = DEFAULT_ZIP_EXTRACTION_LIMITS,
) -> None:
    """Extract zip members into ``dest`` after a complete safety preflight."""
    dest = dest.resolve()
    members = zf.infolist()
    validate_zip_members(zf, limits)
    for info in members:
        name = info.filename
        target = (dest / name).resolve()
        try:
            target.relative_to(dest)
        except ValueError as exc:
            raise ValueError(
                f"Refusing to extract zip member outside destination: {name!r}"
            ) from exc
        if info.is_dir() or name.endswith("/"):
            target.mkdir(parents=True, exist_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        with zf.open(info, "r") as src, open(target, "wb") as out:
            shutil.copyfileobj(src, out)


def count_images(images_dir: Path) -> int:
    """Count image files under ``images_dir`` (known extensions, case-insensitive)."""
    if not images_dir.is_dir():
        return 0
    count = 0
    for path in images_dir.iterdir():
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            count += 1
    return count


def count_labels(labels_dir: Path) -> int:
    """Count ``.txt`` label files under ``labels_dir`` (case-insensitive suffix)."""
    if not labels_dir.is_dir():
        return 0
    return sum(1 for p in labels_dir.iterdir() if p.is_file() and p.suffix.lower() == ".txt")


def find_image_for_stem(images_dir: Path, stem: str) -> Path | None:
    """Return first image under *images_dir* matching *stem* with a known extension.

    Search is case-insensitive on the extension (e.g. ``.JPG``, ``.Png``).
    Preference order: .jpg, .jpeg, .png, .bmp, .webp (then mixed-case via scan).
    """
    for ext in _PREFERRED_IMAGE_EXTS:
        if ext not in IMAGE_EXTENSIONS:
            continue
        candidate = images_dir / f"{stem}{ext}"
        if candidate.is_file():
            return candidate
        candidate_upper = images_dir / f"{stem}{ext.upper()}"
        if candidate_upper.is_file():
            return candidate_upper

    # Case-insensitive scan for mixed-case extensions (e.g. .Jpg)
    stem_lower = stem.lower()
    if not images_dir.is_dir():
        return None
    for path in images_dir.iterdir():
        if not path.is_file():
            continue
        if path.stem.lower() != stem_lower:
            continue
        if path.suffix.lower() in IMAGE_EXTENSIONS:
            return path
    return None


def resolve_dataset_root(output_dir: Path) -> Path:
    """If extract produced a single top-level dir with images/labels, use that root."""
    images = output_dir / "images"
    labels = output_dir / "labels"
    if images.is_dir() and labels.is_dir():
        return output_dir

    # Nested: single subdirectory containing images/ + labels/
    try:
        children = [p for p in output_dir.iterdir() if p.is_dir() and not p.name.startswith(".")]
    except FileNotFoundError:
        return output_dir

    if len(children) == 1:
        nested = children[0]
        if (nested / "images").is_dir() and (nested / "labels").is_dir():
            print(f"  Nested dataset root detected: {nested.name}/")
            return nested

    # Partial / multi-root: prefer first child that has both images and labels
    for child in children:
        if (child / "images").is_dir() and (child / "labels").is_dir():
            print(f"  Using nested dataset root: {child.name}/")
            return child

    return output_dir


def resolve_self_train_dirs(root: Path) -> tuple[Path, Path] | None:
    """Return (images_dir, labels_dir) for self-train; train-only or flat, never val.

    Prefer ``images/train`` + ``labels/train`` when both exist. Fall back to
    flat ``images`` + ``labels`` only when there is no train split tree.
    """
    root = Path(root)
    train_images = root / "images" / "train"
    train_labels = root / "labels" / "train"
    if train_images.is_dir() and train_labels.is_dir():
        return train_images, train_labels
    images = root / "images"
    labels = root / "labels"
    # Flat layout only when there is no train split tree.
    if images.is_dir() and labels.is_dir() and not train_images.is_dir():
        return images, labels
    return None
