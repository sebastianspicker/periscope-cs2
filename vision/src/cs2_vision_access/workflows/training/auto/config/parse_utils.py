"""Primitive parsers for auto-train config."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.contracts import normalize_class_names

from .types import AutoTrainConfigError


def _optional_path(raw: object, field_name: str) -> Path | None:
    if raw is None or raw == "":
        return None
    if not isinstance(raw, str):
        raise AutoTrainConfigError(f"{field_name} must be a string path or null")
    return Path(raw)


def _require_mapping(raw: object, field_name: str) -> dict[str, Any]:
    if raw is None:
        return {}
    if not isinstance(raw, dict):
        raise AutoTrainConfigError(f"{field_name} must be an object")
    return raw


def _parse_class_names(raw: object) -> dict[int, str]:
    """Parse class names; default PRODUCT_CLASSES via normalize_class_names."""
    if raw is None:
        return normalize_class_names(None, default="product")
    if isinstance(raw, list):
        values: dict[int, str] | dict[str, str] = {index: value for index, value in enumerate(raw)}
    elif isinstance(raw, Mapping):
        values = {str(key): str(value) for key, value in raw.items()}
    else:
        raise AutoTrainConfigError("class_names must be a list or mapping")
    try:
        out = normalize_class_names(values)
    except ValueError as error:
        raise AutoTrainConfigError(str(error)) from error
    if set(out) != set(range(len(out))):
        raise AutoTrainConfigError("class_names ids must be contiguous and start at zero")
    return out


def _optional_float(raw: object, field_name: str) -> float | None:
    if raw is None:
        return None
    if isinstance(raw, bool) or not isinstance(raw, (int, float)):
        raise AutoTrainConfigError(f"{field_name} must be a number or null")
    return float(raw)


def _optional_int(raw: object, field_name: str) -> int | None:
    """Parse optional int; rejects bools. Allows negative (e.g. AutoBatch -1)."""
    if raw is None:
        return None
    if isinstance(raw, bool) or not isinstance(raw, int):
        raise AutoTrainConfigError(f"{field_name} must be an int or null")
    return raw


def _parse_conf(raw: object, field_name: str, default: float) -> float:
    value = raw if raw is not None else default
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise AutoTrainConfigError(f"{field_name} must be a number in [0, 1]")
    conf = float(value)
    if not 0.0 <= conf <= 1.0:
        raise AutoTrainConfigError(f"{field_name} must be in [0, 1]")
    return conf
