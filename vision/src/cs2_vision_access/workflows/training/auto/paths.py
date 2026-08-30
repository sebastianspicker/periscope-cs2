"""Run directory layout under ``artifacts/auto/<run_id>/``."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class RunPaths:
    """Resolved filesystem layout for one auto-train run."""

    run_dir: Path
    state_path: Path
    config_snapshot_path: Path
    data_dir: Path
    extracted_dir: Path
    staging_dir: Path
    dataset_dir: Path
    train_runs_dir: Path
    models_dir: Path
    package_dir: Path
    cloud_dir: Path
    report_path: Path

    def ensure(self) -> None:
        """Create the run tree (idempotent)."""
        for path in (
            self.run_dir,
            self.data_dir,
            self.extracted_dir,
            self.staging_dir,
            self.dataset_dir,
            self.train_runs_dir,
            self.models_dir,
            self.package_dir,
            self.cloud_dir,
        ):
            path.mkdir(parents=True, exist_ok=True)


def build_run_paths(work_root: str | Path, run_id: str) -> RunPaths:
    """Map ``work_root/run_id`` onto the standard auto-train layout."""
    if not isinstance(run_id, str) or not run_id.strip():
        raise ValueError("run_id must be a non-empty string")
    safe_id = run_id.strip()
    if "/" in safe_id or "\\" in safe_id or safe_id in {".", ".."}:
        raise ValueError("run_id must be a single path segment")
    run_dir = Path(work_root) / safe_id
    return RunPaths(
        run_dir=run_dir,
        state_path=run_dir / "state.json",
        config_snapshot_path=run_dir / "config.snapshot.json",
        data_dir=run_dir / "data",
        extracted_dir=run_dir / "data" / "extracted",
        staging_dir=run_dir / "data" / "staging",
        dataset_dir=run_dir / "data" / "dataset",
        train_runs_dir=run_dir / "train_runs",
        models_dir=run_dir / "models",
        package_dir=run_dir / "package",
        cloud_dir=run_dir / "cloud",
        report_path=run_dir / "report.json",
    )
