"""Canonical immutable live settings and persistent configuration boundary.

``LiveSettings`` is the application name for the v1 ``AppConfig`` document.
Keeping the same value object means CLI and GUI retain their established v1
read/write format while sharing one immutable resolved settings model.
"""

from cs2_vision_access.application.configuration.io import (
    find_config_path,
    load_config,
    save_config,
)
from cs2_vision_access.application.configuration.models import (
    CONFIG_FILENAME,
    CONFIG_SCHEMA_VERSION,
    AppConfig,
    ConfigError,
    DisplayConfig,
    InputConfig,
    ModelConfig,
    OutlineConfig,
)

LiveSettings = AppConfig

__all__ = [
    "AppConfig",
    "CONFIG_FILENAME",
    "CONFIG_SCHEMA_VERSION",
    "ConfigError",
    "DisplayConfig",
    "InputConfig",
    "LiveSettings",
    "ModelConfig",
    "OutlineConfig",
    "find_config_path",
    "load_config",
    "save_config",
]
