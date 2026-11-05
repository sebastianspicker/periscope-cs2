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


_ROOT_MANIFESTS = frozenset({"dataset.yaml", "train.txt", "val.txt", "sessions.json"})

# Minimum image count for a real holdout; below this val == train.
HOLDOUT_MIN_IMAGES = 5


def split_holdout(image_names: list[str]) -> tuple[list[str], list[str]]:
    """Return ``(train, val)``: sorted names, every 5th into val, the rest train.

    With fewer than :data:`HOLDOUT_MIN_IMAGES` names, val equals train (metrics
    are then train-only and callers must warn).
    """
    ordered = sorted(image_names)
    if len(ordered) < HOLDOUT_MIN_IMAGES:
        return ordered, list(ordered)
    val = ordered[4::5]
    held = set(val)
    return [n for n in ordered if n not in held], val


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


def is_os_metadata_member(name: str) -> bool:
    """macOS archive debris (``__MACOSX/``, ``.DS_Store``): skipped, not rejected."""
    parts = Path(name.replace("\\", "/")).parts
    return bool(parts) and (parts[0] == "__MACOSX" or parts[-1] == ".DS_Store")


def is_allowed_zip_member(name: str) -> bool:
    """Allowlist: dirs, ``images/**`` images, ``labels/**/*.txt``, root manifests.

    Root manifests: ``dataset.yaml``, ``train.txt``, ``val.txt``, ``sessions.json``.

    One optional top-level wrapper folder is tolerated (see ``resolve_dataset_root``).
    Everything else (notably ``*.cache``, which Ultralytics unpickles) is rejected.
    """
    if name.endswith("/"):
        return True
    parts = Path(name.replace("\\", "/")).parts
    if len(parts) > 1 and parts[0] not in ("images", "labels"):
        parts = parts[1:]
    suffix = Path(parts[-1]).suffix.lower()
    if len(parts) == 1 and parts[0] in _ROOT_MANIFESTS:
        return True
    if len(parts) >= 2 and parts[0] == "images":
        return suffix in IMAGE_EXTENSIONS
    return len(parts) >= 2 and parts[0] == "labels" and suffix == ".txt"


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
        if is_os_metadata_member(name):
            continue  # Finder debris: skipped, never extracted
        if is_unsafe_zip_member(name):
            raise ValueError(
                f"Refusing to extract unsafe zip member (path traversal or absolute path): {name!r}"
            )
        if not is_allowed_zip_member(name):
            raise ValueError(
                "Refusing to extract unexpected zip member (only images/, labels/*.txt and "
                f"dataset.yaml are allowed; *.cache files are never accepted): {name!r}"
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
        if is_os_metadata_member(name):
            continue  # Finder debris: skipped, never extracted
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
