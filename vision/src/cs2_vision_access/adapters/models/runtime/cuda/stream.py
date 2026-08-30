"""CUDA stream management for asynchronous GPU pipeline.

ONNX Runtime (and CuPy) can operate on non-default CUDA streams, allowing:

- Frame N+1 preprocessing (letterbox + upload) while Frame N inference runs.
- Frame N inference while Frame N-1 post-processing (decode + NMS) runs.
- All transfers on a dedicated transfer stream.

This module creates a set of streams and provides context managers for
synchronisation points.
"""

from __future__ import annotations

import logging
from typing import Any

from cs2_vision_access.adapters.models.runtime.cuda.utils import cuda_available

logger = logging.getLogger(__name__)

# Stream roles
STREAM_DEFAULT = 0  # ONNX Runtime inference (default stream)
STREAM_TRANSFER = 1  # CPU ↔ GPU memory transfers
STREAM_COMPUTE = 2  # Post-processing (decode, NMS, contour)

_N_STREAMS = 3

# Module-level singleton
_STREAMS: list[Any] | None = None


def _get_cuda() -> Any:
    """Lazy-import CuPy and return the module."""
    import cupy as cp

    return cp


def get_streams() -> list[Any]:
    """Return the 3 CUDA streams used by the pipeline.

    Returns ``[default, transfer, compute]``.
    """
    global _STREAMS  # noqa: PLW0603
    if _STREAMS is None:
        if not cuda_available():
            raise RuntimeError("CUDA streams require a CUDA-capable GPU")
        cp = _get_cuda()
        _STREAMS = [cp.cuda.Stream(non_blocking=True) for _ in range(_N_STREAMS)]
        logger.info("Created %d CUDA streams for async pipeline", _N_STREAMS)
    return _STREAMS


def sync_all() -> None:
    """Synchronise all CUDA streams (block until all work completes)."""
    if _STREAMS is None:
        return
    for s in _STREAMS:
        s.synchronize()


def sync_stream(stream_id: int) -> None:
    """Synchronise a single stream by ID."""
    if _STREAMS is None:
        return
    _STREAMS[stream_id].synchronize()


# ---------------------------------------------------------------------------
# Context manager for staging work on a specific stream
# ---------------------------------------------------------------------------


class on_stream:  # noqa: N801 — lowercase for context-manager ergonomics
    """Context manager that runs CuPy operations on a specific stream.

    Usage::

        with on_stream(STREAM_TRANSFER):
            # This cupy operation runs on the transfer stream
            gpu_tensor = cp.asarray(cpu_array)

        # The next operation on the default stream will wait for transfer
        # completion automatically (via CuPy's stream synchronisation).
    """

    def __init__(self, stream_id: int) -> None:
        self.stream_id = stream_id
        self._cp = _get_cuda()

    def __enter__(self) -> Any:
        if _STREAMS is not None:
            self._prev = self._cp.cuda.get_current_stream()
            _STREAMS[self.stream_id].use()
        return self

    def __exit__(self, *args: object) -> None:
        if _STREAMS is not None:
            self._prev.use()


# ---------------------------------------------------------------------------
# Pipeline synchronisation helper
# ---------------------------------------------------------------------------


class PipelineSync:
    """Manages async pipeline stages with event-based synchronisation.

    Each stage can signal completion (``done``) and wait for the next
    stage to be ready (``wait_for``).

    Usage::

        sync = PipelineSync()
        # ... start stage 1 ...
        sync.done(0)         # stage 0 complete
        # ... start stage 1 ...
        sync.await_stage(1)  # wait until stage 1 is done
    """

    def __init__(self, n_stages: int = 3) -> None:
        self._events: list[Any] = []
        if cuda_available():
            cp = _get_cuda()
            self._events = [cp.cuda.Event() for _ in range(n_stages)]

    def done(self, stage: int) -> None:
        """Record that ``stage`` has finished."""
        if self._events:
            self._events[stage].record()

    def await_stage(self, stage: int) -> None:
        """Block until ``stage`` has finished."""
        if self._events:
            self._events[stage].synchronize()


__all__ = [
    "STREAM_COMPUTE",
    "STREAM_DEFAULT",
    "STREAM_TRANSFER",
    "PipelineSync",
    "get_streams",
    "on_stream",
    "sync_all",
    "sync_stream",
]
