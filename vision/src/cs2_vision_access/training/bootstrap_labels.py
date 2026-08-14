"""Bootstrap initial silhouette labels with COCO person YOLO-seg.

Used when a dataset has images but few/no labels so the first fine-tune has
polygon shapes to learn. Shared by remote autonomous loops and auto-train
``stage_label``.
"""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path
from typing import Any

from cs2_vision_access.training.contracts import (
    PRODUCT_CLASSES,
    Layout,
    write_dataset_yaml,
)
from cs2_vision_access.training.dataset_zip import find_image_for_stem
from cs2_vision_access.training.self_train import (
    qc_yolo_seg_lines,
    write_label,
)
from cs2_vision_access.training.train_core import auto_train_device


def bootstrap_class_id(class_map: Mapping[int, Any]) -> int:
    """Prefer class 0 (product player); else first key."""
    if not class_map:
        return 0
    return 0 if 0 in class_map else int(next(iter(sorted(class_map.keys()))))


def _ensure_dirs(data_dir: Path) -> tuple[Path, Path]:
    images = data_dir / "images"
    labels = data_dir / "labels"
    images.mkdir(parents=True, exist_ok=True)
    labels.mkdir(parents=True, exist_ok=True)
    return images, labels


def _list_image_stems(images_dir: Path) -> list[str]:
    from cs2_vision_access.dataset.types import IMAGE_EXTENSIONS

    stems: list[str] = []
    if not images_dir.is_dir():
        return stems
    for path in sorted(images_dir.iterdir()):
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            stems.append(path.stem)
    return stems


def bootstrap_labels_with_yolo_person(
    data_dir: str | Path,
    *,
    device: str | None = None,
    conf: float = 0.25,
    base_model: str = "yolo11n-seg.pt",
    class_id: int = 0,
    max_images: int = 0,
    min_polygon_area: float = 1e-6,
    images_dir: str | Path | None = None,
    labels_dir: str | Path | None = None,
    classes: Mapping[int, str] | None = None,
    layout: Layout | str = Layout.FLAT_BOOTSTRAP,
    write_yaml: bool = True,
) -> int:
    """Create initial silhouette labels using COCO person YOLO-seg.

    Used when a zip has images but few/no labels so the first fine-tune has
    polygon shapes to learn. Class id is remapped to *class_id* (default 0
    for player / single-class training).

    Person misses are **not** written as empty labels — frames stay unlabeled so
    later self-train can fill them. Written polygons use the ``# pseudo`` marker
    via :func:`write_label` (``overwrite_pseudo``).

    Optional *images_dir* / *labels_dir* override the default flat
    ``data_dir/images`` + ``data_dir/labels`` layout (e.g. session_split
    ``images/train`` + ``labels/train``). When *write_yaml* is True, rewrite
    ``dataset.yaml`` under *data_dir* using *layout* and *classes* (or a
    single-class product map derived from *class_id*).
    """
    import numpy as np
    from ultralytics import YOLO

    from cs2_vision_access.training.prepare_lib import candidate_image_dirs_for_labeling

    data_dir = Path(data_dir)
    if images_dir is None and labels_dir is None:
        images_dir, labels_dir = _ensure_dirs(data_dir)
    else:
        images_dir = Path(images_dir) if images_dir is not None else data_dir / "images"
        labels_dir = Path(labels_dir) if labels_dir is not None else data_dir / "labels"
        images_dir.mkdir(parents=True, exist_ok=True)
        labels_dir.mkdir(parents=True, exist_ok=True)
    device = device or auto_train_device()
    model = YOLO(base_model)

    # Flat images/ plus optional CS2-10k holdout staging dir images_val/
    # (only when labeling the default flat pool — not an explicit train split).
    default_images = (data_dir / "images").resolve()
    include_val = images_dir.resolve() == default_images
    image_roots = candidate_image_dirs_for_labeling(
        data_dir,
        images_dir=images_dir,
        include_images_val=include_val,
    )
    if not image_roots and images_dir.is_dir():
        # Empty primary dir: still scan it so max_images / loops stay defined.
        image_roots = [images_dir]

    stems_with_root: list[tuple[str, Path]] = []
    seen_stems: set[str] = set()
    for root in image_roots:
        for stem in _list_image_stems(root):
            if stem in seen_stems:
                continue
            seen_stems.add(stem)
            stems_with_root.append((stem, root))

    written = 0
    for i, (stem, stem_root) in enumerate(stems_with_root):
        if max_images > 0 and i >= max_images:
            break
        label_path = labels_dir / f"{stem}.txt"
        if label_path.is_file() and label_path.stat().st_size > 0:
            # Leave gold / existing non-empty labels alone.
            try:
                first = label_path.read_text(encoding="utf-8").splitlines()[:1]
                if first and not first[0].strip().startswith("# pseudo"):
                    continue
            except OSError:
                continue
            # Pseudo may still be revised only if empty of polygons — skip if body.
            body = label_path.read_text(encoding="utf-8")
            content_lines = [
                ln for ln in body.splitlines() if ln.strip() and not ln.strip().startswith("#")
            ]
            if content_lines:
                continue

        image_path = find_image_for_stem(stem_root, stem)
        if image_path is None:
            continue

        results = model.predict(
            source=str(image_path),
            conf=conf,
            device=device,
            verbose=False,
            classes=[0],  # COCO person
        )
        if not results:
            # Leave unlabeled — self-train can fill later.
            continue

        result = results[0]
        h, w = (
            int(result.orig_shape[0]),
            int(result.orig_shape[1]),
        )
        lines: list[str] = []
        if getattr(result, "masks", None) is not None and result.masks is not None:
            xy = getattr(result.masks, "xy", None)
            if xy is not None:
                for poly in xy:
                    arr = np.asarray(poly, dtype=np.float64)
                    if arr.ndim != 2 or arr.shape[0] < 3:
                        continue
                    coords: list[str] = []
                    for x, y in arr:
                        nx = min(1.0, max(0.0, float(x) / float(w)))
                        ny = min(1.0, max(0.0, float(y) / float(h)))
                        coords.append(f"{nx:.6f}")
                        coords.append(f"{ny:.6f}")
                    if len(coords) >= 6:
                        lines.append(f"{class_id} " + " ".join(coords))
        if not lines and getattr(result, "boxes", None) is not None and result.boxes is not None:
            xyxy = result.boxes.xyxy.cpu().numpy()
            for box in xyxy:
                x1, y1, x2, y2 = (float(v) for v in box)
                pts = [
                    (x1 / w, y1 / h),
                    (x2 / w, y1 / h),
                    (x2 / w, y2 / h),
                    (x1 / w, y2 / h),
                ]
                coords = []
                for nx, ny in pts:
                    coords.append(f"{min(1.0, max(0.0, nx)):.6f}")
                    coords.append(f"{min(1.0, max(0.0, ny)):.6f}")
                lines.append(f"{class_id} " + " ".join(coords))

        lines = qc_yolo_seg_lines(
            lines,
            allowed_class_ids={class_id},
            min_area=float(min_polygon_area),
        )
        if not lines:
            # No usable polygon — leave unlabeled.
            continue

        if write_label(label_path, lines, policy="overwrite_pseudo"):
            written += 1
            if written % 25 == 0:
                print(f"  bootstrap labels: {written} frames...")

    # Ensure yaml matches class map for shape training (skip when caller rewrites).
    if write_yaml:
        class_map = (
            dict(classes)
            if classes is not None
            else (PRODUCT_CLASSES if class_id == 0 else {class_id: "player"})
        )
        write_dataset_yaml(
            data_dir,
            layout=layout,
            classes=class_map,
            portable_path=False,
        )
    print(f"✓ Bootstrap wrote {written} new labels via {base_model} (person→class {class_id})")
    return written


__all__ = ["bootstrap_class_id", "bootstrap_labels_with_yolo_person"]
