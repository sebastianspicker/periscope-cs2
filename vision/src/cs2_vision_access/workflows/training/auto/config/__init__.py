"""Auto-train configuration loading (JSON / YAML)."""

from __future__ import annotations

from .load import config_from_mapping, load_config
from .types import (
    SCHEMA_VERSION,
    SUPPORTED_BACKENDS,
    SUPPORTED_LABEL_TEACHERS,
    SUPPORTED_MODES,
    AutoTrainConfig,
    AutoTrainConfigError,
    EvalConfig,
    ExportConfig,
    HumanGateConfig,
    LabelConfig,
    PathsConfig,
    ResumeConfig,
    SelfTrainConfig,
    SourcesConfig,
    TrainConfig,
)

__all__ = [
    "SCHEMA_VERSION",
    "SUPPORTED_BACKENDS",
    "SUPPORTED_LABEL_TEACHERS",
    "SUPPORTED_MODES",
    "AutoTrainConfig",
    "AutoTrainConfigError",
    "EvalConfig",
    "ExportConfig",
    "HumanGateConfig",
    "LabelConfig",
    "PathsConfig",
    "ResumeConfig",
    "SelfTrainConfig",
    "SourcesConfig",
    "TrainConfig",
    "config_from_mapping",
    "load_config",
]
