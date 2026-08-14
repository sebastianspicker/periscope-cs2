"""Dataset label analysis and analysis plots for progress reports."""

from __future__ import annotations

from collections import Counter
from collections.abc import Mapping
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

# Palette for class bars / boxes (cycles if more classes than colors).
_CLASS_COLORS = [
    "#1f77b4",
    "#ff7f0e",
    "#2ca02c",
    "#d62728",
    "#9467bd",
    "#8c564b",
    "#e377c2",
    "#7f7f7f",
    "#bcbd22",
    "#17becf",
]


@dataclass
class LabelAnalysis:
    """Aggregate statistics from a YOLO label directory."""

    class_counts: dict[int, int]
    centers: list[tuple[float, float]]  # cx, cy normalized
    sizes: list[tuple[float, float]]  # w, h normalized
    boxes: list[tuple[float, float, float, float, int]]  # x1,y1,x2,y2,cls
    n_files: int
    n_empty: int
    # Instances whose class_id was not in the provided class_names map.
    unknown_class_counts: dict[int, int] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def _parse_label_line(
    tokens: list[str],
) -> tuple[int, float, float, float, float, float, float, float, float] | None:
    """Parse one YOLO-det or YOLO-seg line → ``(cls, cx, cy, w, h, x1, y1, x2, y2)``."""
    if len(tokens) < 5:
        return None
    try:
        cls = int(float(tokens[0]))
    except (TypeError, ValueError):
        return None
    if cls < 0:
        return None

    # Detection: class xc yc w h
    if len(tokens) == 5:
        try:
            xc, yc, w, h = (float(t) for t in tokens[1:5])
        except (TypeError, ValueError):
            return None
        if w <= 0.0 or h <= 0.0:
            return None
        x1 = xc - w / 2.0
        y1 = yc - h / 2.0
        x2 = xc + w / 2.0
        y2 = yc + h / 2.0
        return cls, xc, yc, w, h, x1, y1, x2, y2

    # Segmentation: class x1 y1 x2 y2 ... (even number of coordinate tokens)
    coords = tokens[1:]
    if len(coords) < 6 or len(coords) % 2 != 0:
        return None
    try:
        xs = [float(coords[i]) for i in range(0, len(coords), 2)]
        ys = [float(coords[i]) for i in range(1, len(coords), 2)]
    except (TypeError, ValueError):
        return None
    if not xs or not ys:
        return None
    x1, x2 = min(xs), max(xs)
    y1, y2 = min(ys), max(ys)
    w = x2 - x1
    h = y2 - y1
    if w <= 0.0 or h <= 0.0:
        return None
    cx = (x1 + x2) / 2.0
    cy = (y1 + y2) / 2.0
    return cls, cx, cy, w, h, x1, y1, x2, y2


def analyze_labels(
    labels_dir: Path,
    class_names: Mapping[int, str] | None = None,
) -> LabelAnalysis:
    """Parse YOLO-seg or YOLO-det ``.txt`` labels under ``labels_dir``.

    - det: ``class xc yc w h``
    - seg: ``class x1 y1 x2 y2 ...`` → bbox from min/max

    Bad lines are skipped. Coordinates are expected normalized in ``[0, 1]``.

    When ``class_names`` is provided, instances whose class id is not a key of
    that mapping are excluded from the main aggregates and recorded under
    ``unknown_class_counts`` instead (filtering + validation).
    """
    root = Path(labels_dir)
    class_counts: Counter[int] = Counter()
    unknown_class_counts: Counter[int] = Counter()
    centers: list[tuple[float, float]] = []
    sizes: list[tuple[float, float]] = []
    boxes: list[tuple[float, float, float, float, int]] = []
    n_files = 0
    n_empty = 0
    allowed: set[int] | None = set(int(k) for k in class_names) if class_names else None

    if not root.is_dir():
        return LabelAnalysis(
            class_counts={},
            centers=[],
            sizes=[],
            boxes=[],
            n_files=0,
            n_empty=0,
            unknown_class_counts={},
        )

    for path in sorted(root.rglob("*.txt")):
        if not path.is_file():
            continue
        n_files += 1
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            n_empty += 1
            continue

        instances = 0
        for line in text.splitlines():
            tokens = line.split()
            if not tokens:
                continue
            parsed = _parse_label_line(tokens)
            if parsed is None:
                continue
            cls, cx, cy, w, h, x1, y1, x2, y2 = parsed
            if allowed is not None and cls not in allowed:
                unknown_class_counts[cls] += 1
                instances += 1
                continue
            class_counts[cls] += 1
            centers.append((cx, cy))
            sizes.append((w, h))
            boxes.append((x1, y1, x2, y2, cls))
            instances += 1

        if instances == 0:
            n_empty += 1

    return LabelAnalysis(
        class_counts=dict(sorted(class_counts.items())),
        centers=centers,
        sizes=sizes,
        boxes=boxes,
        n_files=n_files,
        n_empty=n_empty,
        unknown_class_counts=dict(sorted(unknown_class_counts.items())),
    )


def _class_label(cls: int, class_names: Mapping[int, str] | None) -> str:
    if class_names and cls in class_names:
        return f"{cls}: {class_names[cls]}"
    return str(cls)


def _color_for_class(cls: int) -> str:
    return _CLASS_COLORS[cls % len(_CLASS_COLORS)]


def plot_dataset_analysis(
    analysis: LabelAnalysis,
    out_path: Path,
    *,
    class_names: Mapping[int, str] | None = None,
    max_boxes_draw: int = 400,
) -> Path:
    """Write a 4-panel (2×2) dataset analysis figure as PNG (dpi=120).

    Panels:
    1. Class instance histogram
    2. Sample of axis-aligned boxes on a normalized 0–1 frame
    3. 2D histogram / scatter of box centers
    4. Scatter of width vs height
    """
    destination = Path(out_path)
    destination.parent.mkdir(parents=True, exist_ok=True)

    fig, axes = plt.subplots(2, 2, figsize=(10, 8))
    ax_hist, ax_boxes, ax_centers, ax_sizes = axes.ravel()

    # --- 1. Class histogram ---
    if analysis.class_counts:
        classes = sorted(analysis.class_counts.keys())
        counts = [analysis.class_counts[c] for c in classes]
        labels = [_class_label(c, class_names) for c in classes]
        colors = [_color_for_class(c) for c in classes]
        ax_hist.bar(range(len(classes)), counts, color=colors, edgecolor="black", linewidth=0.4)
        ax_hist.set_xticks(range(len(classes)))
        ax_hist.set_xticklabels(labels, rotation=30, ha="right", fontsize=8)
    else:
        ax_hist.text(
            0.5, 0.5, "no instances", ha="center", va="center", transform=ax_hist.transAxes
        )
        ax_hist.set_xticks([])
    ax_hist.set_ylabel("instances")
    ax_hist.set_title("Class distribution")
    ax_hist.set_xlabel("class")

    # --- 2. Sample boxes ---
    ax_boxes.set_xlim(0, 1)
    ax_boxes.set_ylim(1, 0)  # image-like y-down
    ax_boxes.set_aspect("equal")
    ax_boxes.set_title(f"Boxes (sample ≤{max_boxes_draw})")
    ax_boxes.set_xlabel("x")
    ax_boxes.set_ylabel("y")
    n_draw = min(len(analysis.boxes), max(0, int(max_boxes_draw)))
    if n_draw > 0 and len(analysis.boxes) > n_draw:
        # Uniform subsample for dense overlays
        indices = np.linspace(0, len(analysis.boxes) - 1, n_draw, dtype=int)
        sample = [analysis.boxes[i] for i in indices]
    else:
        sample = analysis.boxes[:n_draw]
    for x1, y1, x2, y2, cls in sample:
        rect = plt.Rectangle(
            (x1, y1),
            max(x2 - x1, 1e-6),
            max(y2 - y1, 1e-6),
            fill=False,
            edgecolor=_color_for_class(cls),
            linewidth=0.8,
            alpha=0.75,
        )
        ax_boxes.add_patch(rect)
    if not sample:
        ax_boxes.text(0.5, 0.5, "no boxes", ha="center", va="center", transform=ax_boxes.transAxes)

    # --- 3. Centers ---
    ax_centers.set_xlim(0, 1)
    ax_centers.set_ylim(1, 0)
    ax_centers.set_aspect("equal")
    ax_centers.set_title("Box centers")
    ax_centers.set_xlabel("cx")
    ax_centers.set_ylabel("cy")
    if analysis.centers:
        cx = np.array([c[0] for c in analysis.centers], dtype=float)
        cy = np.array([c[1] for c in analysis.centers], dtype=float)
        if len(cx) >= 8:
            ax_centers.hist2d(cx, cy, bins=30, range=[[0, 1], [0, 1]], cmap="Blues")
            # hist2d uses y-up; flip to match image coords
            ax_centers.set_ylim(1, 0)
        else:
            ax_centers.scatter(cx, cy, s=12, c="#1f77b4", alpha=0.7, edgecolors="none")
    else:
        ax_centers.text(
            0.5, 0.5, "no centers", ha="center", va="center", transform=ax_centers.transAxes
        )

    # --- 4. Width vs height ---
    ax_sizes.set_title("Box width vs height")
    ax_sizes.set_xlabel("width")
    ax_sizes.set_ylabel("height")
    ax_sizes.set_xlim(0, 1)
    ax_sizes.set_ylim(0, 1)
    if analysis.sizes:
        ws = np.array([s[0] for s in analysis.sizes], dtype=float)
        hs = np.array([s[1] for s in analysis.sizes], dtype=float)
        ax_sizes.scatter(ws, hs, s=12, c="#ff7f0e", alpha=0.65, edgecolors="none")
    else:
        ax_sizes.text(0.5, 0.5, "no sizes", ha="center", va="center", transform=ax_sizes.transAxes)

    fig.suptitle(
        f"Dataset labels — files={analysis.n_files}, empty={analysis.n_empty}, "
        f"instances={sum(analysis.class_counts.values())}",
        fontsize=11,
    )
    fig.tight_layout()
    fig.savefig(destination, dpi=120, bbox_inches="tight")
    plt.close(fig)
    return destination
