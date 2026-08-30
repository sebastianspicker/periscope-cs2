"""Shared Ultralytics train argument assembly (local/cloud adapters)."""

from __future__ import annotations

import logging
from typing import Any

LOGGER = logging.getLogger(__name__)


def build_train_kwargs(
    *,
    data: str,
    epochs: int,
    imgsz: int,
    batch: int,
    device: str,
    project: str,
    name: str,
    exist_ok: bool = True,
    plots: bool = True,
    resume: bool = False,
    lr0: float | None = None,
    patience: int | None = None,
    workers: int | None = 2,
    amp: bool | None = None,
    seed: int = 42,
    verbose: bool = True,
    val: bool = True,
) -> dict[str, Any]:
    """Assemble keyword arguments for ``YOLO.train``.

    Optional knobs (``lr0``, ``patience``, ``workers``, ``amp``) are omitted
    when ``None`` so callers can leave Ultralytics defaults in place.
    ``resume`` is only included when True.
    """
    kwargs: dict[str, Any] = {
        "data": data,
        "epochs": epochs,
        "imgsz": imgsz,
        "batch": batch,
        "device": device,
        "project": project,
        "name": name,
        "exist_ok": exist_ok,
        "plots": plots,
        "seed": seed,
        "verbose": verbose,
        "val": val,
    }
    if resume:
        kwargs["resume"] = True
    if lr0 is not None:
        kwargs["lr0"] = lr0
    if patience is not None:
        kwargs["patience"] = patience
    if workers is not None:
        kwargs["workers"] = workers
    if amp is not None:
        kwargs["amp"] = amp
    return kwargs


def is_oom_error(exc: BaseException) -> bool:
    """Return True if *exc* looks like a CUDA / allocator out-of-memory error."""
    msg = str(exc).lower()
    return "out of memory" in msg or "cuda out of memory" in msg


def next_batch_on_oom(batch: int, exc: BaseException) -> int | None:
    """If *exc* is OOM, return a halved batch size (min 1); else ``None``.

    Used by cloud / autonomous trainers to rebuild train kwargs and retry.
    """
    if not is_oom_error(exc):
        return None
    return max(1, int(batch) // 2)


def auto_train_device() -> str:
    """Pick ``cuda:0`` when torch reports a GPU, otherwise ``cpu``."""
    try:
        import torch

        if torch.cuda.is_available():
            return "cuda:0"
    except (ImportError, OSError, RuntimeError) as error:
        # Missing/broken CUDA runtimes are recoverable: local and cloud
        # training both support an explicit CPU device.
        LOGGER.debug("CUDA autodetection unavailable; using CPU: %s", error)
    return "cpu"
