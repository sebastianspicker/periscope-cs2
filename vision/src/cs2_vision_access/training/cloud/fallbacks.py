"""Soft-import vendored helpers so cloud training works when pasted/vendored alone.

When the full ``cs2_vision_access`` package is available, re-export the real
implementations. Otherwise provide local fallbacks matching the same APIs so
notebooks and HF Space can vendor this package without the whole monorepo.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import tempfile
import zipfile
from pathlib import Path

# Soft-import model_manifest so this package works when vendored alone (Colab / HF).
try:
    from cs2_vision_access.model_manifest import create_manifest as write_model_manifest
    from cs2_vision_access.model_manifest import sha256_file
except ImportError:

    def _sha256_file(path: str | Path) -> str:
        digest = hashlib.sha256()
        with Path(path).open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
        return digest.hexdigest()

    def _write_model_manifest(
        model_path: str | Path,
        manifest_path: str | Path,
        *,
        classes: dict[int, str],
        origin: str,
        license_name: str,
        overwrite: bool = False,
    ) -> Path:
        """Local fallback matching model_manifest.create_manifest JSON schema."""
        model = Path(model_path)
        if not model.is_file():
            raise FileNotFoundError(f"model is not a regular file: {model}")
        if model.suffix.lower() != ".onnx":
            raise ValueError("model must use the .onnx format")

        destination = Path(manifest_path)
        if destination.suffix.lower() != ".json":
            raise ValueError("manifest destination must use the .json extension")
        if destination.exists() and not overwrite:
            raise FileExistsError(
                f"manifest already exists: {destination}; pass overwrite=True to replace it"
            )

        class_payload = {
            str(key): str(value)
            for key, value in sorted(classes.items(), key=lambda item: int(item[0]))
        }
        payload = {
            "schema_version": 1,
            "model_filename": model.name,
            "sha256": _sha256_file(model),
            "task": "instance-segmentation",
            "classes": class_payload,
            "origin": str(origin).strip(),
            "license": str(license_name).strip(),
        }
        text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary_name: str | None = None
        try:
            with tempfile.NamedTemporaryFile(
                "w",
                encoding="utf-8",
                dir=destination.parent,
                prefix=f".{destination.name}.",
                suffix=".tmp",
                delete=False,
            ) as handle:
                temporary_name = handle.name
                handle.write(text)
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(temporary_name, destination)
            temporary_name = None
        finally:
            if temporary_name is not None:
                Path(temporary_name).unlink(missing_ok=True)
        return destination

    sha256_file = _sha256_file
    write_model_manifest = _write_model_manifest

# Soft-import dataset zip helpers so this package works when vendored alone.
try:
    from cs2_vision_access.training.dataset_zip import (
        IMAGE_EXTENSIONS as _IMAGE_EXTENSIONS,
    )
    from cs2_vision_access.training.dataset_zip import (
        count_images as _count_images,
    )
    from cs2_vision_access.training.dataset_zip import (
        count_labels as _count_labels,
    )
    from cs2_vision_access.training.dataset_zip import (
        is_unsafe_zip_member as _is_unsafe_zip_member,
    )
    from cs2_vision_access.training.dataset_zip import (
        resolve_dataset_root as _resolve_dataset_root,
    )
    from cs2_vision_access.training.dataset_zip import (
        safe_extract_zip as _safe_extract_zip,
    )
except ImportError:
    # Inline fallbacks for standalone import / notebook paste of cloud alone.
    _IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png"}

    def _is_unsafe_zip_member(name: str) -> bool:
        """Return True if a zip member path is absolute or uses path traversal."""
        if not name or name.endswith("/"):
            pass
        normalised = name.replace("\\", "/")
        if normalised.startswith("/") or re.match(r"^[A-Za-z]:", normalised):
            return True
        parts = Path(normalised).parts
        return bool(any(part == ".." for part in parts))

    def _safe_extract_zip(zf: zipfile.ZipFile, dest: Path) -> None:
        """Extract zip members into ``dest``, rejecting traversal / absolute paths."""
        dest = dest.resolve()
        for info in zf.infolist():
            name = info.filename
            if _is_unsafe_zip_member(name):
                raise ValueError(
                    "Refusing to extract unsafe zip member "
                    f"(path traversal or absolute path): {name!r}"
                )
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

    def _count_images(images_dir: Path) -> int:
        """Count image files under ``images_dir`` (.jpg/.jpeg/.png, case-insensitive)."""
        if not images_dir.is_dir():
            return 0
        count = 0
        for path in images_dir.iterdir():
            if path.is_file() and path.suffix.lower() in _IMAGE_EXTENSIONS:
                count += 1
        return count

    def _count_labels(labels_dir: Path) -> int:
        if not labels_dir.is_dir():
            return 0
        return sum(1 for p in labels_dir.iterdir() if p.is_file() and p.suffix.lower() == ".txt")

    def _resolve_dataset_root(output_dir: Path) -> Path:
        """If extract produced a single top-level dir with images/labels, use that root."""
        images = output_dir / "images"
        labels = output_dir / "labels"
        if images.is_dir() and labels.is_dir():
            return output_dir
        try:
            children = [
                p for p in output_dir.iterdir() if p.is_dir() and not p.name.startswith(".")
            ]
        except FileNotFoundError:
            return output_dir
        if len(children) == 1:
            nested = children[0]
            if (nested / "images").is_dir() and (nested / "labels").is_dir():
                print(f"  Nested dataset root detected: {nested.name}/")
                return nested
        for child in children:
            if (child / "images").is_dir() and (child / "labels").is_dir():
                print(f"  Using nested dataset root: {child.name}/")
                return child
        return output_dir

# ---------------------------------------------------------------------------
# Constants sources (SSOT: training.contracts)
# ---------------------------------------------------------------------------

try:
    from cs2_vision_access.training.contracts import PROFILES as _PROFILES
    from cs2_vision_access.training.contracts import VOMBIT_CLASSES as _VOMBIT_CLASSES
except ImportError:
    # Inline fallback when cloud is vendored alone (notebook paste).
    _VOMBIT_CLASSES = {
        0: "ct",
        1: "ct_head",
        2: "t",
        3: "t_head",
    }
    _PROFILES = None  # type: ignore[assignment]

# Soft-import shared train helpers (vendored notebooks may omit train_core).
try:
    from cs2_vision_access.training.train_core import (
        auto_train_device as _auto_train_device,
    )
    from cs2_vision_access.training.train_core import (
        build_train_kwargs as _build_train_kwargs,
    )
    from cs2_vision_access.training.train_core import (
        next_batch_on_oom as _next_batch_on_oom,
    )
except ImportError:
    _build_train_kwargs = None  # type: ignore[assignment]
    _auto_train_device = None  # type: ignore[assignment]
    _next_batch_on_oom = None  # type: ignore[assignment]
