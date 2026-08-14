"""Load/save outline preferences JSON with path safety checks."""

from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path

from cs2_vision_access.prefs.coerce import _from_mapping
from cs2_vision_access.prefs.errors import OutlinePreferencesError
from cs2_vision_access.prefs.model import OutlinePreferences
from cs2_vision_access.renderer import OutlineStyle

_REQUIRED_KEYS = frozenset({"schema_version", "preset"})
_OPTIONAL_KEYS = frozenset(
    {
        "inner_color",
        "outer_color",
        "inner_width",
        "outer_width",
        "fill_opacity",
        "scale_with_frame",
        "stroke_pattern",
        "dash_period_px",
        "fill_mode",
        "halo_blur",
        "adapt_width_to_area",
        "outline_kernel",
    }
)
_ALLOWED_KEYS = _REQUIRED_KEYS | _OPTIONAL_KEYS


def load_outline_preferences(path: str | Path) -> OutlinePreferences:
    """Load and validate outline preferences from a local JSON file."""
    prefs_path = _regular_json_file(path, label="preferences")
    try:
        raw = json.loads(prefs_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise OutlinePreferencesError(f"could not read preferences: {error}") from error
    if not isinstance(raw, dict):
        raise OutlinePreferencesError("preferences root must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_REQUIRED_KEYS - keys)
    unknown = sorted(keys - _ALLOWED_KEYS)
    if missing or unknown:
        raise OutlinePreferencesError(
            f"preferences keys do not match schema; missing={missing}, unknown={unknown}"
        )
    return _from_mapping(raw)


def load_outline_style(path: str | Path) -> OutlineStyle:
    """Load preferences and resolve them to a validated ``OutlineStyle``."""
    return load_outline_preferences(path).resolve()


def save_outline_preferences(
    path: str | Path,
    preferences: OutlinePreferences | OutlineStyle,
    *,
    overwrite: bool = False,
) -> Path:
    """Atomically write outline preferences as schema-versioned JSON."""
    if isinstance(preferences, OutlineStyle):
        preferences = OutlinePreferences.from_style(preferences)

    destination = Path(path)
    if destination.suffix.lower() != ".json":
        raise OutlinePreferencesError("preferences destination must use the .json extension")
    if destination.is_symlink():
        raise OutlinePreferencesError("preferences destination must not be a symlink")
    if destination.exists() and not destination.is_file():
        raise OutlinePreferencesError("preferences destination exists and is not a regular file")
    if destination.exists() and not overwrite:
        raise OutlinePreferencesError(
            f"preferences already exist: {destination}; pass overwrite=True to replace"
        )
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise OutlinePreferencesError("preferences parent must not be a symlink")

    payload = json.dumps(preferences.as_json(), indent=2, sort_keys=True) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, destination)
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return destination.resolve()


def _regular_json_file(path: str | Path, *, label: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise OutlinePreferencesError(f"{label} path must not be a symlink")
    if candidate.parent.is_symlink():
        raise OutlinePreferencesError(f"{label} parent must not be a symlink")
    if not candidate.is_file():
        raise OutlinePreferencesError(f"{label} is not a regular file: {candidate}")
    if candidate.suffix.lower() != ".json":
        raise OutlinePreferencesError(f"{label} must use the .json format")
    return candidate.resolve()
