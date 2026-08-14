"""Ultralytics results.csv helpers for progress reports."""

from __future__ import annotations

import csv
import re
from pathlib import Path

# Map normalized column stems → preferred short metric keys.
_METRIC_ALIASES: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"^(metrics/)?precision(\([BbMm]\))?$", re.I), "precision"),
    (re.compile(r"^(metrics/)?recall(\([BbMm]\))?$", re.I), "recall"),
    (re.compile(r"^(metrics/)?mAP50-95(\([BbMm]\))?$", re.I), "mAP50-95"),
    (re.compile(r"^(metrics/)?mAP50(\([BbMm]\))?$", re.I), "mAP50"),
    (re.compile(r"^(metrics/)?mAP_0\.5:0\.95(\([BbMm]\))?$", re.I), "mAP50-95"),
    (re.compile(r"^(metrics/)?mAP_0\.5(\([BbMm]\))?$", re.I), "mAP50"),
    (re.compile(r"^train/box_loss$", re.I), "train/box_loss"),
    (re.compile(r"^val/box_loss$", re.I), "val/box_loss"),
    (re.compile(r"^train/cls_loss$", re.I), "train/cls_loss"),
    (re.compile(r"^val/cls_loss$", re.I), "val/cls_loss"),
    (re.compile(r"^train/dfl_loss$", re.I), "train/dfl_loss"),
    (re.compile(r"^val/dfl_loss$", re.I), "val/dfl_loss"),
    (re.compile(r"^train/seg_loss$", re.I), "train/seg_loss"),
    (re.compile(r"^val/seg_loss$", re.I), "val/seg_loss"),
    (re.compile(r"^epoch$", re.I), "epoch"),
]


def _normalize_metric_key(column: str) -> str | None:
    name = column.strip()
    if not name:
        return None
    for pattern, key in _METRIC_ALIASES:
        if pattern.match(name):
            return key
    # Keep slash-prefixed train/val losses and metrics/* generically
    lowered = name.lower()
    if lowered.startswith("train/") or lowered.startswith("val/"):
        return name if "/" in name else lowered
    if lowered.startswith("metrics/"):
        # Strip metrics/ and (B)/(M) suffix for a short key when not aliased
        short = re.sub(r"^metrics/", "", name, flags=re.I)
        short = re.sub(r"\([BbMm]\)$", "", short)
        return short
    return None


def parse_ultralytics_results_csv(results_csv: Path) -> dict[str, float]:
    """Return the last row of an Ultralytics ``results.csv`` as metric floats.

    Keys are normalized (e.g. ``metrics/mAP50(B)`` → ``mAP50``).
    Non-numeric cells are skipped. Missing file / empty CSV → ``{}``.
    """
    path = Path(results_csv)
    if not path.is_file():
        return {}

    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            if reader.fieldnames is None:
                return {}
            # Normalize fieldnames (Ultralytics often pads with spaces)
            field_map = {raw: raw.strip() for raw in reader.fieldnames}
            last_row: dict[str, str] | None = None
            for row in reader:
                last_row = row
    except (OSError, UnicodeDecodeError, csv.Error):
        return {}

    if not last_row:
        return {}

    metrics: dict[str, float] = {}
    for raw_key, value in last_row.items():
        if raw_key is None:
            continue
        col = field_map.get(raw_key, raw_key).strip()
        key = _normalize_metric_key(col)
        if key is None:
            continue
        if value is None:
            continue
        text = str(value).strip()
        if not text:
            continue
        try:
            metrics[key] = float(text)
        except ValueError:
            continue
    return metrics


def find_ultralytics_run_dir(project_root: Path, name: str | None = None) -> Path | None:
    """Find the newest run directory containing ``results.csv``.

    Search root is ``project_root/name`` when ``name`` is set, else ``project_root``.
    """
    root = Path(project_root)
    if name:
        root = root / name
    if not root.is_dir():
        return None

    candidates: list[Path] = []
    results = root / "results.csv"
    if results.is_file():
        candidates.append(root)

    try:
        for child in root.rglob("results.csv"):
            if child.is_file():
                parent = child.parent
                if parent not in candidates:
                    candidates.append(parent)
    except OSError:
        pass

    if not candidates:
        return None

    def _mtime(p: Path) -> float:
        try:
            return (p / "results.csv").stat().st_mtime
        except OSError:
            return 0.0

    return max(candidates, key=_mtime)
