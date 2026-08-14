"""Autonomous training progress reports with dataset plots and training curves.

Produces Ultralytics-style dataset analysis figures and markdown progress
summaries from YOLO label directories and training ``results.csv`` files.
"""

from __future__ import annotations

from cs2_vision_access.training.progress_report.labels import (
    LabelAnalysis,
    analyze_labels,
    plot_dataset_analysis,
)
from cs2_vision_access.training.progress_report.metrics import (
    find_ultralytics_run_dir,
    parse_ultralytics_results_csv,
)
from cs2_vision_access.training.progress_report.report_md import write_progress_report

__all__ = [
    "LabelAnalysis",
    "analyze_labels",
    "plot_dataset_analysis",
    "parse_ultralytics_results_csv",
    "find_ultralytics_run_dir",
    "write_progress_report",
]
