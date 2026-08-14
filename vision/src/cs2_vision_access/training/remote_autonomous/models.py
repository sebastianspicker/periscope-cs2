"""Constants, report dataclasses, and internal phase result types."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from cs2_vision_access.training.contracts import Layout

# CS2-10k provenance (also noted when materialising that source).
_CS2_10K_LICENSE = "CC BY-NC 4.0"
_CS2_10K_SOURCE = "https://huggingface.co/datasets/RekaAI/CS2-10k"
_STATE_FILENAME = "autonomous_state.json"
_DEFAULT_SPLIT_SEED = 42
_CS2_10K_HOLDOUT_JSON = "cs2_10k_holdout_videos.json"
_DEFAULT_CS2_10K_HOLDOUT_VIDEO_FRACTION = 0.2


@dataclass
class IterationReport:
    iteration: int
    onnx: str
    manifest: str
    labeled_before: int
    labeled_after: int
    self_train_accepted: int
    self_train_rejected_low_conf: int
    map50: float | None = None
    map50_95: float | None = None


@dataclass
class AutonomousReport:
    data_dir: str
    onnx_path: str
    manifest_path: str
    bundle_path: str | None
    iterations: list[IterationReport] = field(default_factory=list)
    bootstrap_labels_written: int = 0
    final_label_count: int = 0
    final_image_count: int = 0
    device: str = "cpu"
    origin: str = ""
    notes: list[str] = field(default_factory=list)
    progress_dir: str | None = None
    progress_report_md: str | None = None
    best_iteration: int | None = None
    best_map: float | None = None
    best_onnx_path: str | None = None
    status: str = "ok"  # "ok" | "degraded" | "failed"
    dataset_license: str | None = None
    dataset_source: str | None = None

    def as_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "data_dir": self.data_dir,
            "onnx_path": self.onnx_path,
            "manifest_path": self.manifest_path,
            "bundle_path": self.bundle_path,
            "bootstrap_labels_written": self.bootstrap_labels_written,
            "final_label_count": self.final_label_count,
            "final_image_count": self.final_image_count,
            "device": self.device,
            "origin": self.origin,
            "notes": list(self.notes),
            "iterations": [asdict(i) for i in self.iterations],
            "progress_dir": self.progress_dir,
            "progress_report_md": self.progress_report_md,
            "best_iteration": self.best_iteration,
            "best_map": self.best_map,
            "best_onnx_path": self.best_onnx_path,
            "status": self.status,
            "dataset_license": self.dataset_license,
            "dataset_source": self.dataset_source,
        }


@dataclass
class _ResolvedData:
    """Result of :func:`_phase_resolve_data`."""

    data_dir: Path
    images_dir: Path
    labels_dir: Path
    n_images: int
    n_labels: int
    dataset_license: str | None
    dataset_source: str | None


@dataclass
class _HeldOutSplitResult:
    """Result of :func:`_phase_held_out_split`."""

    train_images_dir: Path
    train_labels_dir: Path
    layout: Layout


@dataclass
class _TrainLoopResult:
    """Result of :func:`_phase_train_self_train_loop`."""

    status: str
    batch_size: int
    onnx_path: Path
    manifest_path: Path
    iteration_reports: list[IterationReport]
    best_iteration: int | None
    best_map: float | None
    best_onnx_path: Path | None
    best_manifest_path: Path | None
    progress_dir: Path | None
    progress_report_md: Path | None
    labels_at_loop_start: int
    self_train_hard_errors: list[str]


def _iteration_report_from_dict(raw: Mapping[str, Any]) -> IterationReport | None:
    """Best-effort reconstruct :class:`IterationReport` from saved state."""
    try:
        return IterationReport(
            iteration=int(raw["iteration"]),
            onnx=str(raw.get("onnx", "")),
            manifest=str(raw.get("manifest", "")),
            labeled_before=int(raw.get("labeled_before", 0)),
            labeled_after=int(raw.get("labeled_after", 0)),
            self_train_accepted=int(raw.get("self_train_accepted", 0)),
            self_train_rejected_low_conf=int(raw.get("self_train_rejected_low_conf", 0)),
            map50=(float(raw["map50"]) if raw.get("map50") is not None else None),
            map50_95=(float(raw["map50_95"]) if raw.get("map50_95") is not None else None),
        )
    except (KeyError, TypeError, ValueError):
        return None
