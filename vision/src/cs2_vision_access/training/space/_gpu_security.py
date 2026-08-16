"""Trusted GPU capability probes for the Hugging Face Space."""

from __future__ import annotations

import logging
import os
import stat
import subprocess
from collections.abc import Callable, Iterable
from pathlib import Path

TRUSTED_NVIDIA_SMI_ENV = "CS2_VISION_TRUSTED_NVIDIA_SMI_PATH"
PLATFORM_NVIDIA_SMI_PATHS = (
    Path("/usr/bin/nvidia-smi"),
    Path("/usr/local/bin/nvidia-smi"),
    Path("/usr/local/nvidia/bin/nvidia-smi"),
    Path("/usr/lib/wsl/lib/nvidia-smi"),
)

PathResolver = Callable[[], str | None]


def validated_nvidia_smi_path(candidate: Path) -> str | None:
    """Return a safe executable path, or ``None`` for an untrusted candidate."""
    if not candidate.is_absolute():
        return None
    try:
        resolved = candidate.resolve(strict=True)
        mode = resolved.stat().st_mode
    except OSError:
        return None
    if (
        not stat.S_ISREG(mode)
        or mode & (stat.S_IWGRP | stat.S_IWOTH)
        or not os.access(resolved, os.X_OK)
    ):
        return None
    return str(resolved)


def nvidia_smi_path(
    configured: str | None,
    platform_paths: Iterable[Path],
) -> str | None:
    """Resolve a validated executable without consulting ambient PATH."""
    if configured:
        return validated_nvidia_smi_path(Path(configured))
    for candidate in platform_paths:
        executable = validated_nvidia_smi_path(candidate)
        if executable is not None:
            return executable
    return None


def has_cuda(resolve_executable: PathResolver, logger: logging.Logger) -> bool:
    """Check whether a CUDA-capable GPU is available."""
    try:
        import torch

        return bool(torch.cuda.is_available())
    except (ImportError, OSError, RuntimeError) as error:
        logger.debug("Torch CUDA probe unavailable; trying nvidia-smi: %s", error)
        executable = resolve_executable()
        if executable is None:
            return False
        try:
            return (
                subprocess.run([executable], capture_output=True, check=False, timeout=3).returncode
                == 0
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            logger.debug("nvidia-smi CUDA probe failed; using CPU: %s", error)
            return False


def gpu_info(resolve_executable: PathResolver, logger: logging.Logger) -> str:
    """Return a short GPU description or ``None (CPU)``."""
    try:
        import torch

        if torch.cuda.is_available():
            name = torch.cuda.get_device_name(0)
            vram = torch.cuda.get_device_properties(0).total_memory / 1e9
            return f"{name} ({vram:.1f} GB)"
    except (ImportError, OSError, RuntimeError) as error:
        logger.debug("Torch GPU probe unavailable; trying nvidia-smi: %s", error)
    executable = resolve_executable()
    if executable is None:
        return "None (CPU)"
    try:
        result = subprocess.run(
            [executable, "--query-gpu=name,memory.total", "--format=csv,noheader"],
            capture_output=True,
            text=True,
            check=False,
            timeout=3,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip().split(",")[0].strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        logger.debug("nvidia-smi GPU info probe failed; reporting CPU: %s", error)
    return "None (CPU)"
