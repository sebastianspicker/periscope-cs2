"""Local outline preference load/save (JSON schema v1).

Preferences store a named preset plus optional field overrides. Resolution
reuses ``OutlineStyle`` validation (contrast ≥3:1, fill cap, width geometry).
Unknown JSON keys fail closed. Defaults match the high-visibility preset.

Public API is re-exported from submodules so existing imports continue to work::

    from cs2_vision_access.prefs import OutlinePreferences, load_outline_preferences
"""

from __future__ import annotations

from cs2_vision_access.prefs.errors import OutlinePreferencesError
from cs2_vision_access.prefs.io import (
    load_outline_preferences,
    load_outline_style,
    save_outline_preferences,
)
from cs2_vision_access.prefs.model import (
    DEFAULT_PRESET,
    SCHEMA_VERSION,
    OutlinePreferences,
    default_outline_preferences,
)

__all__ = [
    "SCHEMA_VERSION",
    "DEFAULT_PRESET",
    "OutlinePreferencesError",
    "OutlinePreferences",
    "default_outline_preferences",
    "load_outline_preferences",
    "load_outline_style",
    "save_outline_preferences",
]
