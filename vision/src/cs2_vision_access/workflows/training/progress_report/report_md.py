"""Markdown progress report writer."""

from __future__ import annotations

import contextlib
import json
import shutil
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.progress_report.labels import (
    LabelAnalysis,
    analyze_labels,
    plot_dataset_analysis,
)
from cs2_vision_access.workflows.training.progress_report.metrics import (
    parse_ultralytics_results_csv,
)


def _json_safe_analysis(analysis: LabelAnalysis) -> dict[str, Any]:
    """Serialize LabelAnalysis; keep list fields for inspection."""
    return {
        "class_counts": {str(k): v for k, v in analysis.class_counts.items()},
        "n_files": analysis.n_files,
        "n_empty": analysis.n_empty,
        "n_instances": sum(analysis.class_counts.values()),
        "n_centers": len(analysis.centers),
        "n_boxes": len(analysis.boxes),
        # Compact samples for debugging (full lists can be huge)
        "centers_sample": analysis.centers[:50],
        "sizes_sample": analysis.sizes[:50],
    }


def _format_metric_table(metrics: Mapping[str, float]) -> list[str]:
    if not metrics:
        return ["_(no metrics)_", ""]
    preferred = [
        "epoch",
        "precision",
        "recall",
        "mAP50",
        "mAP50-95",
        "train/box_loss",
        "val/box_loss",
        "train/cls_loss",
        "val/cls_loss",
        "train/seg_loss",
        "val/seg_loss",
    ]
    keys = [k for k in preferred if k in metrics]
    for k in sorted(metrics.keys()):
        if k not in keys:
            keys.append(k)
    lines = [
        "| Metric | Value |",
        "| --- | ---: |",
    ]
    for key in keys:
        val = metrics[key]
        if abs(val - round(val)) < 1e-9 and abs(val) < 1e6:
            rendered = str(int(round(val))) if key == "epoch" else f"{val:.6g}"
        else:
            rendered = f"{val:.6g}"
        lines.append(f"| `{key}` | {rendered} |")
    lines.append("")
    return lines


def write_progress_report(
    progress_dir: Path,
    *,
    labels_dir: Path,
    class_names: Mapping[int, str] | None = None,
    iterations: list[dict[str, Any]] | None = None,
    title: str = "Autonomous training progress",
    notes: list[str] | None = None,
    images_count: int = 0,
) -> Path:
    """Create a progress directory with analysis plots, metrics, and ``report.md``.

    Writes:
    - ``labels_summary.json``
    - ``dataset_analysis.png``
    - per-iteration ``metrics_iter_XX.json`` and ``train_curves_iter_XX.png``
    - ``report.md`` with relative image links and metric tables

    Returns the path to ``report.md``.
    """
    out = Path(progress_dir)
    out.mkdir(parents=True, exist_ok=True)

    analysis = analyze_labels(Path(labels_dir), class_names=class_names)
    summary = _json_safe_analysis(analysis)
    if class_names:
        summary["class_names"] = {str(k): v for k, v in class_names.items()}
    summary["images_count"] = int(images_count)
    summary["labels_dir"] = str(Path(labels_dir))

    summary_path = out / "labels_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    plot_path = out / "dataset_analysis.png"
    plot_dataset_analysis(
        analysis,
        plot_path,
        class_names=class_names,
    )

    iter_blocks: list[str] = []
    for raw in iterations or []:
        if not isinstance(raw, Mapping):
            continue
        try:
            idx = int(raw.get("iteration", raw.get("iter", len(iter_blocks) + 1)))
        except (TypeError, ValueError):
            idx = len(iter_blocks) + 1
        tag = f"{idx:02d}"

        metrics: dict[str, float] = {}
        results_csv = raw.get("results_csv")
        if results_csv:
            metrics = parse_ultralytics_results_csv(Path(str(results_csv)))
            (out / f"metrics_iter_{tag}.json").write_text(
                json.dumps(metrics, indent=2) + "\n",
                encoding="utf-8",
            )
            # Prefer explicit results.png; else sibling of results.csv
            curves_src: Path | None = None
            if raw.get("results_png"):
                candidate = Path(str(raw["results_png"]))
                if candidate.is_file():
                    curves_src = candidate
            if curves_src is None:
                sibling = Path(str(results_csv)).with_name("results.png")
                if sibling.is_file():
                    curves_src = sibling
            if curves_src is not None:
                dest_curves = out / f"train_curves_iter_{tag}.png"
                with contextlib.suppress(OSError):
                    shutil.copy2(curves_src, dest_curves)

        # Carry through optional labeling stats for the markdown section
        labeled_before = raw.get("labeled_before")
        labeled_after = raw.get("labeled_after")
        extra_notes = raw.get("notes")

        section: list[str] = [f"## Iteration {idx}", ""]
        if labeled_before is not None or labeled_after is not None:
            section.append(
                f"- Labeled before: **{labeled_before}**  \n- Labeled after: **{labeled_after}**"
            )
            section.append("")
        if metrics:
            section.append("### Metrics (last epoch)")
            section.append("")
            section.extend(_format_metric_table(metrics))
        curves_name = f"train_curves_iter_{tag}.png"
        if (out / curves_name).is_file():
            section.append("### Training curves")
            section.append("")
            section.append(f"![Training curves iteration {idx}]({curves_name})")
            section.append("")
        if extra_notes:
            if isinstance(extra_notes, str):
                section.append(f"- {extra_notes}")
            elif isinstance(extra_notes, list):
                for note in extra_notes:
                    section.append(f"- {note}")
            section.append("")
        # Dump remaining scalar fields for traceability
        skip = {
            "iteration",
            "iter",
            "results_csv",
            "results_png",
            "labeled_before",
            "labeled_after",
            "notes",
        }
        extras: dict[str, Any] = {}
        for k, v in raw.items():
            if k in skip:
                continue
            if isinstance(v, (str, int, float, bool)) or v is None:
                extras[k] = v
        if extras:
            section.append("### Iteration fields")
            section.append("")
            section.append("```json")
            section.append(json.dumps(extras, indent=2))
            section.append("```")
            section.append("")
        iter_blocks.append("\n".join(section))

    # --- report.md ---
    lines: list[str] = [
        f"# {title}",
        "",
        f"- Images: **{images_count}**",
        f"- Label files: **{analysis.n_files}** (empty: **{analysis.n_empty}**)",
        f"- Instances: **{sum(analysis.class_counts.values())}**",
        f"- Labels dir: `{Path(labels_dir)}`",
        "",
        "## Class counts",
        "",
    ]
    if analysis.class_counts:
        lines.extend(["| Class | Name | Count |", "| ---: | --- | ---: |"])
        for cls, count in sorted(analysis.class_counts.items()):
            name = class_names.get(cls, "") if class_names else ""
            lines.append(f"| {cls} | {name} | {count} |")
        lines.append("")
    else:
        lines.append("_No labeled instances found._")
        lines.append("")

    lines.extend(
        [
            "## Dataset analysis",
            "",
            "![Dataset analysis](dataset_analysis.png)",
            "",
        ]
    )

    if notes:
        lines.append("## Notes")
        lines.append("")
        for note in notes:
            lines.append(f"- {note}")
        lines.append("")

    if iter_blocks:
        lines.append("---")
        lines.append("")
        lines.extend(iter_blocks)
    else:
        lines.append("_No training iterations attached._")
        lines.append("")

    report_path = out / "report.md"
    report_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    return report_path
