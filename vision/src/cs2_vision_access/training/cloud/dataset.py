"""Dataset discovery, extraction, yaml helpers, and output packaging."""

from __future__ import annotations

import re
import zipfile
from collections.abc import Mapping
from pathlib import Path

from .constants import _DEFAULT_ZIP_SEARCH_ROOTS, DEFAULT_CLASSES
from .fallbacks import (
    _count_images,
    _count_labels,
    _resolve_dataset_root,
    _safe_extract_zip,
)


def _classes_to_yaml_names(classes: Mapping[int, str] | Mapping[str, str]) -> dict[int, str]:
    """Normalise class maps keyed by int or str to ``dict[int, str]``."""
    out: dict[int, str] = {}
    for key, name in classes.items():
        idx = int(key)
        out[idx] = str(name)
    return out


def _format_dataset_yaml(data_dir: Path, classes: Mapping[int, str]) -> str:
    """Build dataset.yaml text with absolute path and class names."""
    names_block = "\n".join(f"  {idx}: {name}" for idx, name in sorted(classes.items()))
    return (
        f"path: {data_dir.resolve().as_posix()}\n"
        f"train: images\n"
        f"val: images\n"
        f"nc: {len(classes)}\n"
        f"names:\n"
        f"{names_block}\n"
    )


def _ensure_dataset_yaml(
    data_dir: Path,
    classes: Mapping[int, str] | Mapping[str, str] | None = None,
) -> Path:
    """Ensure ``dataset.yaml`` exists with a sensible class map.

    Prefers an existing yaml (rewrites ``path`` to absolute). If missing,
    writes a default from ``classes`` or :data:`DEFAULT_CLASSES`.
    Never derives ``nc`` from the number of label files.
    """
    yaml_path = data_dir / "dataset.yaml"
    class_map = _classes_to_yaml_names(classes) if classes is not None else dict(DEFAULT_CLASSES)

    if yaml_path.is_file():
        text = yaml_path.read_text(encoding="utf-8")
        # Update path line to absolute dataset root; keep names/nc if present.
        abs_path = data_dir.resolve().as_posix()
        if re.search(r"(?m)^path:\s*", text):
            text = re.sub(r"(?m)^path:\s*.*$", f"path: {abs_path}", text, count=1)
        else:
            text = f"path: {abs_path}\n{text}"
        # If caller overrode classes, rewrite nc/names.
        if classes is not None:
            # Strip existing nc/names blocks and append override.
            text = re.sub(r"(?m)^nc:\s*.*\n?", "", text)
            text = re.sub(r"(?ms)^names:\s*\n(?:[ \t]+.+\n?)*", "", text)
            names_block = "\n".join(f"  {idx}: {name}" for idx, name in sorted(class_map.items()))
            text = text.rstrip() + f"\nnc: {len(class_map)}\nnames:\n{names_block}\n"
        yaml_path.write_text(text if text.endswith("\n") else text + "\n", encoding="utf-8")
        return yaml_path

    yaml_path.write_text(_format_dataset_yaml(data_dir, class_map), encoding="utf-8")
    print(f"  Created dataset.yaml with {len(class_map)} classes: {list(class_map.values())}")
    return yaml_path


def find_dataset_zip(search_roots: list[str | Path] | None = None) -> Path | None:
    """Search common cloud paths for a dataset ``.zip`` file.

    Args:
        search_roots: Directories to search (non-recursive for files; one level
            of subdirs is also checked). Defaults to Kaggle input, Colab
            content/Drive, CWD, and ``data/``.

    Returns:
        Path to the first matching ``*.zip``, or ``None`` if none found.
        Prefers names containing ``cs2`` or ``dataset`` when multiple exist.
    """
    roots = [
        Path(r) for r in (search_roots if search_roots is not None else _DEFAULT_ZIP_SEARCH_ROOTS)
    ]
    candidates: list[Path] = []

    for root in roots:
        if not root.is_dir():
            continue
        try:
            for path in root.iterdir():
                if path.is_file() and path.suffix.lower() == ".zip":
                    candidates.append(path)
                elif path.is_dir():
                    for child in path.iterdir():
                        if child.is_file() and child.suffix.lower() == ".zip":
                            candidates.append(child)
        except OSError:
            continue

    if not candidates:
        return None

    def _score(p: Path) -> tuple[int, str]:
        name = p.name.lower()
        score = 0
        if "cs2" in name:
            score += 2
        if "dataset" in name or "train" in name:
            score += 1
        return (-score, name)

    candidates.sort(key=_score)
    return candidates[0]


def package_outputs(
    onnx_path: str | Path,
    manifest_path: str | Path,
    output_zip: str | Path,
    *,
    extra_paths: list[str | Path] | None = None,
    extra_dirs: list[tuple[str | Path, str]] | None = None,
) -> Path:
    """Zip ONNX + manifest (+ FP16 sibling if present) for notebook download.

    Looks for ``<stem>-fp16.onnx`` next to ``onnx_path`` and includes it when
    found (e.g. ``cs2-yolo11n-seg.onnx`` → ``cs2-yolo11n-seg-fp16.onnx``).

    Args:
        onnx_path: Path to the FP32 ONNX weights file (required).
        manifest_path: Path to the model manifest JSON (required).
        output_zip: Destination zip path.
        extra_paths: Optional files to add at the zip root using each file's
            basename as the archive name (e.g. progress report assets).
            Missing paths are skipped with a one-line note.
        extra_dirs: Optional ``(directory, arcname_prefix)`` pairs. Every file
            under each directory is walked (``rglob``) and written as
            ``arcname_prefix/relative_path`` with POSIX separators. Missing
            directories are skipped with a one-line note.

    Returns:
        Path to the created zip archive.
    """
    onnx_path = Path(onnx_path)
    manifest_path = Path(manifest_path)
    output_zip = Path(output_zip)
    output_zip.parent.mkdir(parents=True, exist_ok=True)

    if not onnx_path.is_file():
        raise FileNotFoundError(f"ONNX not found: {onnx_path}")
    if not manifest_path.is_file():
        raise FileNotFoundError(f"Manifest not found: {manifest_path}")

    # Sibling FP16: stem-fp16.onnx next to FP32 file
    fp16_path = onnx_path.with_name(f"{onnx_path.stem}-fp16{onnx_path.suffix}")

    with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(onnx_path, arcname=onnx_path.name)
        zf.write(manifest_path, arcname=manifest_path.name)
        if fp16_path.is_file():
            zf.write(fp16_path, arcname=fp16_path.name)
            print(f"  Packaged FP16: {fp16_path.name}")

        for raw in extra_paths or ():
            path = Path(raw)
            if not path.is_file():
                print(f"  (skip extra path, not a file: {path})")
                continue
            zf.write(path, arcname=path.name)
            print(f"  Packaged extra: {path.name}")

        for raw_dir, prefix in extra_dirs or ():
            directory = Path(raw_dir)
            if not directory.is_dir():
                print(f"  (skip extra dir, not a directory: {directory})")
                continue
            # Normalise prefix: strip trailing slashes, use forward slashes.
            prefix_clean = str(prefix).replace("\\", "/").strip("/")
            for file_path in sorted(directory.rglob("*")):
                if not file_path.is_file():
                    continue
                rel = file_path.relative_to(directory).as_posix()
                arcname = f"{prefix_clean}/{rel}" if prefix_clean else rel
                zf.write(file_path, arcname=arcname)
            print(f"  Packaged extra dir: {directory} → {prefix_clean or '.'}/")

    print(f"✓ Package: {output_zip} ({output_zip.stat().st_size / 1e6:.1f} MB)")
    return output_zip


def extract_dataset(zip_path: str | Path, output_dir: str | Path = "cs2_data") -> Path:
    """Extract a dataset zip into the working directory.

    The zip should have the structure produced by
    ``python -m cs2_vision_access.training.bundle``::

        dataset.yaml
        images/frame_00000000.jpg
        labels/frame_00000000.txt

    Safety:
      * Rejects members with absolute paths or ``..`` traversal.
    Nested roots:
      * If the zip has a single top-level directory that contains
        ``images/`` and ``labels/``, that directory is used as the dataset root.

    Images are counted with case-insensitive ``.jpg`` / ``.jpeg`` / ``.png``.

    Returns:
        Path to the resolved dataset root (with ``images/`` and ``labels/``).
    """
    zip_path = Path(zip_path)
    if not zip_path.is_file():
        raise FileNotFoundError(f"Dataset zip not found: {zip_path}")

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Extracting {zip_path} to {output_dir}...")
    with zipfile.ZipFile(zip_path, "r") as zf:
        _safe_extract_zip(zf, output_dir)

    data_root = _resolve_dataset_root(output_dir)

    n_labels = _count_labels(data_root / "labels")
    n_images = _count_images(data_root / "images")
    print(f"✓ {n_labels} labels, {n_images} images (root: {data_root})")

    return data_root
