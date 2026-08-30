"""Shared ONNX Runtime utility functions for segmenter backends."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

from cs2_vision_access.adapters.models.segmenters.protocol import SegmenterError


def find_input_name(available: set[str], *candidates: str) -> str:
    """Return the first candidate that matches an available input name."""
    for candidate in candidates:
        if candidate in available:
            return candidate
    for candidate in candidates:
        for avail in available:
            if candidate.lower() in avail.lower():
                return avail
    raise SegmenterError(
        f"could not find a known input name among {sorted(available)}; expected one of {candidates}"
    )


def detect_io_names(
    session: Any,
    expected_input: str = "images",
    expected_output: str = "output0",
    num_classes: int | None = None,
) -> dict[str, Any]:
    """Discover input/output names and metadata from an ONNX Runtime session.

    Handles the common pattern used across segmenter backends: checking for
    known names (``images``, ``output0``) with fallback to ``next(iter(...))``,
    and inferring the number of classes from the output tensor shape.

    Returns:
        ``{"input_name", "output_name", "num_classes"}``
    """
    inp = {i.name for i in session.get_inputs()}
    out = {o.name for o in session.get_outputs()}

    input_name = expected_input if expected_input in inp else next(iter(inp))
    output_name = expected_output if expected_output in out else next(iter(out))

    if num_classes is None:
        out_shape = session.get_outputs()[0].shape
        num_classes = out_shape[1] - 4 if len(out_shape) == 3 and out_shape[1] is not None else 0

    return {
        "input_name": input_name,
        "output_name": output_name,
        "num_classes": num_classes,
    }


def resolve_model_path(
    explicit: str | Path | None,
    default: Path,
    env_var: str,
    download_url: str = "",
) -> Path:
    """Resolve the path to a model file via explicit path, env var, or default.

    Resolution order:
    1. ``explicit`` path (if provided and exists).
    2. ``env_var`` environment variable.
    3. ``default`` path (if file exists).
    4. Raise ``SegmenterError`` with a download hint.
    """
    if explicit is not None:
        return Path(explicit)
    env_path = os.environ.get(env_var)
    if env_path:
        return Path(env_path)
    if default.is_file():
        return default
    msg = f"Model not found at {default}"
    if download_url:
        msg += f"; download from {download_url}"
    msg += f" or set the {env_var} environment variable"
    raise SegmenterError(msg)
