"""Outer-composed model-runtime operations used by workflows."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any

_recommend_device: Callable[[], str] | None = None
_convert_to_fp16: Callable[..., Any] | None = None


def register_model_runtime(
    *, recommend_device: Callable[[], str], convert_to_fp16: Callable[..., Any]
) -> None:
    """Install volatile GPU/ORT operations at an interface boundary."""
    global _recommend_device, _convert_to_fp16
    _recommend_device = recommend_device
    _convert_to_fp16 = convert_to_fp16


def recommend_device() -> str:
    if _recommend_device is None:
        return "cpu"
    return _recommend_device()


def convert_to_fp16(*args: Any, **kwargs: Any) -> Path:
    if _convert_to_fp16 is None:
        raise RuntimeError("no model runtime is registered; compose an adapter at the interface")
    return Path(_convert_to_fp16(*args, **kwargs))
