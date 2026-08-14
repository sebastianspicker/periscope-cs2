"""Configuration file I/O — discovery, loading, saving, and serialisation.

Separated from data models so that model consumers do not depend on
file-system modules (json, os, tempfile).
"""

from __future__ import annotations

import json
import os
import sys
import tempfile
from dataclasses import asdict
from pathlib import Path
from typing import Any

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


def _user_config_dir() -> Path:
    """Return the platform-appropriate user configuration directory.

    - Windows: ``%APPDATA%\\cs2-vision``
    - macOS / Linux: ``~/.config/cs2-vision``
    """
    if sys.platform == "win32":
        appdata = os.environ.get("APPDATA")
        if appdata:
            return Path(appdata) / "cs2-vision"
    return Path.home() / ".config" / "cs2-vision"


def find_config_path(custom_path: str | Path | None = None) -> Path:
    """Resolve the config file path.

    Resolution order:
    1. Explicit ``custom_path``
    2. ``./cs2-vision-config.json`` (current directory)
    3. ``<user-config-dir>/config.json`` (platform-appropriate user dir)
    """
    if custom_path is not None:
        return Path(custom_path)

    local = Path.cwd() / CONFIG_FILENAME
    if local.is_file():
        return local

    user_dir = _user_config_dir()
    user_file = user_dir / "config.json"
    if user_file.is_file():
        return user_file

    return local  # Default to local; will be created on save


def load_config(path: str | Path | None = None) -> AppConfig:
    """Load configuration from a JSON file, or return defaults if none exists.

    Args:
        path: Path to the config JSON file. If None, searches common locations.

    Returns:
        ``AppConfig`` with values from the file, or defaults.
    """
    resolved = find_config_path(path)
    if not resolved.is_file():
        return AppConfig()

    try:
        raw = json.loads(resolved.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ConfigError(f"could not read config {resolved}: {error}") from error

    return _mapping_to_config(raw)


def save_config(
    config: AppConfig,
    path: str | Path | None = None,
    *,
    overwrite: bool = False,
) -> Path:
    """Save configuration to a JSON file.

    Args:
        config: The configuration to save.
        path: Destination path. If None, uses ``find_config_path``.
        overwrite: When False, raises ``ConfigError`` if the file exists.

    Returns:
        The resolved path of the saved file.
    """
    resolved = find_config_path(path)

    if resolved.exists() and not overwrite:
        raise ConfigError(f"config already exists: {resolved}; pass --overwrite to replace")

    resolved.parent.mkdir(parents=True, exist_ok=True)
    payload = _config_to_mapping(config)

    # Atomic write
    temp_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=resolved.parent,
            prefix=f".{resolved.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temp_name = handle.name
            json.dump(payload, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, resolved)
        temp_name = None
    finally:
        if temp_name is not None:
            Path(temp_name).unlink(missing_ok=True)

    return resolved


# ------------------------------------------------------------------
# Internal serialisation helpers
# ------------------------------------------------------------------


def _mapping_to_config(raw: dict[str, Any]) -> AppConfig:
    """Convert a parsed JSON dict to an AppConfig."""
    inp = raw.get("input", {})
    mdl = raw.get("model", {})
    out = raw.get("outline", {})
    dsp = raw.get("display", {})

    region_raw = inp.get("region")
    region: tuple[int, ...] | None = None
    if isinstance(region_raw, list) and len(region_raw) == 4:
        region = (int(region_raw[0]), int(region_raw[1]), int(region_raw[2]), int(region_raw[3]))

    class_names_raw = mdl.get("class_names", ["person"])
    if isinstance(class_names_raw, list):
        class_names = tuple(str(v) for v in class_names_raw)
    else:
        class_names = ("person",)

    raw_max_dropout = out.get("temporal_max_dropout", 0)
    if (
        isinstance(raw_max_dropout, bool)
        or not isinstance(raw_max_dropout, int)
        or raw_max_dropout < 0
    ):
        temporal_max_dropout = 0
    else:
        temporal_max_dropout = int(raw_max_dropout)

    return AppConfig(
        schema_version=raw.get("schema_version", CONFIG_SCHEMA_VERSION),
        input=InputConfig(
            source_type=str(inp.get("source_type", "screen")),
            device_index=int(inp.get("device_index", 0)),
            device_name=str(inp.get("device_name", "")),
            monitor_index=int(inp.get("monitor_index", 1)),
            region=region,
            file_path=str(inp.get("file_path", "")),
            width=int(inp.get("width", 1920)),
            height=int(inp.get("height", 1080)),
            fps=float(inp.get("fps", 60.0)),
            backend=str(inp.get("backend", "auto")),
        ),
        model=ModelConfig(
            path=str(mdl.get("path", "artifacts/yolo26n-seg.onnx")),
            manifest=str(mdl.get("manifest", "artifacts/yolo26n-seg.model.json")),
            backend=str(mdl.get("backend", "ultralytics-onnx")),
            class_names=class_names,
            confidence=float(mdl.get("confidence", 0.45)),
            image_size=int(mdl.get("image_size", 640)),
            device=str(mdl.get("device", "cpu")),
        ),
        outline=OutlineConfig(
            preset=str(out.get("preset", "maximum-visibility")),
            inner_color=out.get("inner_color"),
            outer_color=out.get("outer_color"),
            inner_width=out.get("inner_width"),
            outer_width=out.get("outer_width"),
            fill_opacity=out.get("fill_opacity"),
            stroke_pattern=out.get("stroke_pattern"),
            output_mode=str(out.get("output_mode", "overlay")),
            alpha_fill=bool(out.get("alpha_fill", False)),
            temporal_enabled=bool(out.get("temporal_enabled", False)),
            temporal_min_frames=int(out.get("temporal_min_frames", 2)),
            temporal_hold=bool(out.get("temporal_hold", False)),
            temporal_max_dropout=temporal_max_dropout,
        ),
        display=DisplayConfig(
            scale=float(dsp.get("scale", 0.5)),
            headless=bool(dsp.get("headless", False)),
            window_title=str(dsp.get("window_title", "CS2 Vision Access - Ingame Overlay")),
            overlay=bool(dsp.get("overlay", False)),
            overlay_x=int(dsp.get("overlay_x", 0)),
            overlay_y=int(dsp.get("overlay_y", 0)),
            overlay_monitor=int(dsp.get("overlay_monitor", 0)),
        ),
    )


def _config_to_mapping(config: AppConfig) -> dict[str, Any]:
    """Convert an AppConfig to a JSON-serializable dict."""
    d = asdict(config)
    # Convert tuple to list for JSON
    if "model" in d and "class_names" in d["model"]:
        d["model"]["class_names"] = list(d["model"]["class_names"])
    if "input" in d and isinstance(d["input"].get("region"), tuple):
        d["input"]["region"] = list(d["input"]["region"])
    return d
