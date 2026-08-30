"""Utility functions for CUDA availability detection and GPU info."""

from __future__ import annotations

import logging
from typing import Any

logger = logging.getLogger(__name__)

# Module-level flag — computed once on first import.
_CUDA_AVAILABLE: bool | None = None


def cuda_available() -> bool:
    """Check whether CuPy + a CUDA-capable GPU are available.

    This is a cached check — the first call probes the system, subsequent
    calls return the cached result.
    """
    global _CUDA_AVAILABLE  # noqa: PLW0603
    if _CUDA_AVAILABLE is not None:
        return _CUDA_AVAILABLE

    try:
        import cupy as cp

        device = cp.cuda.runtime.getDeviceProperties(0)
        major = device["major"]
        _CUDA_AVAILABLE = major >= 5  # compute capability 5.0+
        if _CUDA_AVAILABLE:
            logger.info(
                "CUDA available: %s (compute %d.%d, %d MB VRAM)",
                device["name"].decode(),
                device["major"],
                device["minor"],
                device["totalGlobalMem"] // (1024 * 1024),
            )
        else:
            logger.warning("CUDA device compute capability %d < 5.0 — GPU kernels disabled", major)
    except Exception:
        _CUDA_AVAILABLE = False
        logger.info("CUDA not available — using CPU fallback")

    return _CUDA_AVAILABLE


def gpu_info() -> dict[str, Any]:
    """Return a dict with GPU name, compute capability, and VRAM.

    Returns an empty dict if CUDA is not available.
    """
    if not cuda_available():
        return {}

    import cupy as cp

    try:
        props = cp.cuda.runtime.getDeviceProperties(0)
        return {
            "name": props["name"].decode(),
            "compute_major": props["major"],
            "compute_minor": props["minor"],
            "vram_mb": props["totalGlobalMem"] // (1024 * 1024),
            "multi_processor_count": props["multiProcessorCount"],
        }
    except Exception:
        return {}


__all__ = ["cuda_available", "gpu_info"]
