"""Stage runners for ``session_split`` and ``flat_cloud`` auto-train modes.

Thin facade re-exporting stage modules. Implementation lives in sibling
``stage_*.py`` files; prepare/label/self_train remain in their own modules.
"""

from __future__ import annotations

from .errors import (
    AutoTrainStageError,
    HumanGateBlocked,
)
from .label_stage import stage_label
from .prepare_stage import (
    ExtractDatasetFn,
    ExtractFramesFn,
    stage_prepare_data,
)
from .self_train_stage import stage_self_train
from .stage_eval import stage_eval
from .stage_export import stage_export
from .stage_human_gate import stage_human_gate
from .stage_ingest import stage_ingest
from .stage_report import snapshot_config, stage_report
from .stage_train import (
    TrainCloudFn,
    TrainLocalFn,
    stage_train,
)
from .stage_validate import stage_validate

# Re-export errors for callers that import from stages.
__all__ = [
    "AutoTrainStageError",
    "HumanGateBlocked",
    "ExtractDatasetFn",
    "ExtractFramesFn",
    "TrainCloudFn",
    "TrainLocalFn",
    "snapshot_config",
    "stage_eval",
    "stage_export",
    "stage_human_gate",
    "stage_ingest",
    "stage_label",
    "stage_prepare_data",
    "stage_report",
    "stage_self_train",
    "stage_train",
    "stage_validate",
]
