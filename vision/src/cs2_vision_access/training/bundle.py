#!/usr/bin/env python3
"""Bundle a CS2 training dataset for upload to cloud training platforms.

Usage::

    # After running prepare, bundle for upload:
    uv run python -m cs2_vision_access.training.bundle ^
        --input data/cs2_train ^
        --output data/cs2_train_bundle.zip

    # Custom class names (order = class id 0, 1, ...):
    uv run python -m cs2_vision_access.training.bundle ^
        --input data/cs2_train ^
        --names ct ct_head t t_head
"""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

from cs2_vision_access.training.contracts import VOMBIT_CLASSES
from cs2_vision_access.training.dataset_zip import find_image_for_stem

# Match contracts / dataset.yaml bootstrap defaults (id-order names from VOMBIT_CLASSES).
DEFAULT_NAMES: list[str] = [VOMBIT_CLASSES[i] for i in sorted(VOMBIT_CLASSES)]


def _looks_like_dataset_yaml(text: str) -> bool:
    """Heuristic: enough keys for Ultralytics to load the dataset config."""
    lower = text.lower()
    has_names = "names" in lower or "names:" in text
    has_split = "train" in lower and ("images" in lower or "val" in lower)
    has_nc = "nc:" in lower or "nc " in lower
    return has_names and (has_split or has_nc)


def build_dataset_yaml(
    input_dir: Path,
    names: list[str] | None = None,
) -> str:
    """Return dataset.yaml content: copy valid input yaml (path rewritten) or default."""
    names = list(names) if names is not None else list(DEFAULT_NAMES)
    existing = input_dir / "dataset.yaml"
    if existing.is_file():
        try:
            text = existing.read_text(encoding="utf-8")
        except OSError:
            text = ""
        if text.strip() and _looks_like_dataset_yaml(text):
            # Force path: . so the zip is self-contained on Colab/Kaggle/HF
            lines: list[str] = []
            path_set = False
            for line in text.splitlines():
                stripped = line.strip()
                if stripped.startswith("path:") or stripped.startswith("path "):
                    lines.append("path: .")
                    path_set = True
                else:
                    lines.append(line)
            if not path_set:
                lines.insert(0, "path: .")
            body = "\n".join(lines)
            if not body.endswith("\n"):
                body += "\n"
            return body

    nc = len(names)
    name_lines = "\n".join(f"  {i}: {n}" for i, n in enumerate(names))
    return f"path: .\ntrain: images\nval: images\nnc: {nc}\nnames:\n{name_lines}\n"


def collect_pairs(
    images_dir: Path,
    labels_dir: Path,
    max_frames: int = 0,
) -> list[tuple[Path, Path, str]]:
    """Collect (image_path, label_path, archive_image_name) pairs.

    Skips labels whose matching image is missing. *max_frames* > 0 limits
    how many label files are considered (in sorted order).
    """
    label_files = sorted(labels_dir.glob("*.txt"))
    if max_frames > 0:
        label_files = label_files[:max_frames]

    pairs: list[tuple[Path, Path, str]] = []
    for label_path in label_files:
        stem = label_path.stem
        img_path = find_image_for_stem(images_dir, stem)
        if img_path is None:
            continue
        # Preserve actual extension in the zip (normalized to lowercase)
        ext = img_path.suffix.lower()
        archive_name = f"{stem}{ext}"
        pairs.append((img_path, label_path, archive_name))
    return pairs


def create_bundle(
    input_dir: Path | str,
    output_path: Path | str,
    max_frames: int = 0,
    names: list[str] | None = None,
) -> tuple[int, Path]:
    """Write a zip bundle of images/, labels/, and dataset.yaml.

    Returns (frame_count, output_path).
    Raises SystemExit-style errors via ValueError for empty/invalid input.
    """
    input_dir = Path(input_dir)
    output_path = Path(output_path)
    images_dir = input_dir / "images"
    labels_dir = input_dir / "labels"

    if not images_dir.is_dir() or not labels_dir.is_dir():
        raise ValueError(f"{input_dir} must contain images/ and labels/ directories")

    label_files = list(labels_dir.glob("*.txt"))
    if not label_files:
        raise ValueError(f"no .txt label files found in {labels_dir}")

    pairs = collect_pairs(images_dir, labels_dir, max_frames=max_frames)
    if not pairs:
        raise ValueError(
            "empty bundle: no labeled frames with matching images "
            f"under {images_dir} (checked .jpg/.jpeg/.png)"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    yaml_content = build_dataset_yaml(input_dir, names=names)

    with zipfile.ZipFile(output_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("dataset.yaml", yaml_content)
        for i, (img_path, label_path, archive_name) in enumerate(pairs, start=1):
            stem = label_path.stem
            zf.write(img_path, f"images/{archive_name}")
            zf.write(label_path, f"labels/{stem}.txt")
            if i % 200 == 0:
                print(f"  Packed {i} frames...")

    return len(pairs), output_path


def print_upload_instructions(output_path: Path, size_mb: float, count: int) -> None:
    """Print platform-specific upload guidance after a successful bundle."""
    print(f"\nBundled {count} frames into {output_path} ({size_mb:.1f} MB)")
    print()
    print("Upload instructions:")
    print("-" * 60)
    print("Google Colab")
    print("  1. Open notebooks/colab.ipynb (or File → Upload notebook).")
    print("  2. Run the setup cell, then use files.upload() / drive mount.")
    print(f"  3. Upload {output_path.name} and set DATASET_ZIP to its path.")
    print()
    print("Kaggle")
    print("  1. Create a Dataset at kaggle.com → New Dataset.")
    print(f"  2. Upload {output_path.name} (keep the filename).")
    print("  3. In notebooks/kaggle.ipynb, set DATASET_ZIP to")
    print(f"     /kaggle/input/<your-dataset-slug>/{output_path.name}")
    print()
    print("Hugging Face Space")
    print("  1. Open the CS2 training Space (training/space).")
    print(f"  2. Upload {output_path.name} in the dataset zip field.")
    print("  3. Configure epochs/batch and start training.")
    print("-" * 60)
    print("Module: uv run python -m cs2_vision_access.training.bundle --input <dir> --output <zip>")


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Bundle CS2 training data for cloud upload",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--input",
        default="data/cs2_train",
        help="Dataset directory (images/ + labels/)",
    )
    p.add_argument(
        "--output",
        default="data/cs2_train_bundle.zip",
        help="Output zip path",
    )
    p.add_argument(
        "--max-frames",
        type=int,
        default=0,
        help="Limit to first N labeled frames (0 = all)",
    )
    p.add_argument(
        "--names",
        nargs="+",
        default=None,
        metavar="NAME",
        help=(
            "Class names in id order (default: "
            + " ".join(DEFAULT_NAMES)
            + "). Used only when writing a new dataset.yaml."
        ),
    )
    return p


def main() -> None:
    args = _build_parser().parse_args()
    names = args.names if args.names else None

    try:
        count, output_path = create_bundle(
            input_dir=args.input,
            output_path=args.output,
            max_frames=args.max_frames,
            names=names,
        )
    except ValueError as exc:
        raise SystemExit(f"Error: {exc}") from exc

    size_mb = output_path.stat().st_size / 1e6
    print_upload_instructions(output_path, size_mb, count)


if __name__ == "__main__":
    main()
