"""Persistent configuration for the live ingame overlay pipeline.

Provides data models (``config.models``) and file I/O (``config.io``) for
loading, saving, and discovering JSON configuration files.
"""

from __future__ import annotations

from cs2_vision_access.config.io import (
    _config_to_mapping,
    _mapping_to_config,
    find_config_path,
    load_config,
    save_config,
)
from cs2_vision_access.config.models import (
    CONFIG_FILENAME,
    CONFIG_SCHEMA_VERSION,
    AppConfig,
    ConfigError,
    DisplayConfig,
    InputConfig,
    ModelConfig,
    OutlineConfig,
)

__all__ = [
    "AppConfig",
    "CONFIG_FILENAME",
    "CONFIG_SCHEMA_VERSION",
    "ConfigError",
    "DisplayConfig",
    "InputConfig",
    "ModelConfig",
    "OutlineConfig",
    "_config_to_mapping",
    "_mapping_to_config",
    "find_config_path",
    "load_config",
    "save_config",
]
