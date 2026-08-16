"""Device capability detection — GPU info, VRAM, driver, ORT providers."""

from __future__ import annotations

import os
import platform
import stat
import subprocess
from pathlib import Path
from typing import Any

_TRUSTED_NVIDIA_SMI_ENV = "CS2_VISION_TRUSTED_NVIDIA_SMI_PATH"
_PLATFORM_NVIDIA_SMI_PATHS = (
    Path("/usr/bin/nvidia-smi"),
    Path("/usr/local/bin/nvidia-smi"),
    Path("/usr/local/nvidia/bin/nvidia-smi"),
    Path("/usr/lib/wsl/lib/nvidia-smi"),
    Path("C:/Windows/System32/nvidia-smi.exe"),
    Path("C:/Program Files/NVIDIA Corporation/NVSMI/nvidia-smi.exe"),
)


def _validated_nvidia_smi_path(candidate: Path) -> str | None:
    """Return a validated absolute executable path."""
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


def _nvidia_smi_path() -> str | None:
    """Resolve nvidia-smi without consulting ambient PATH."""
    configured = os.environ.get(_TRUSTED_NVIDIA_SMI_ENV)
    if configured:
        return _validated_nvidia_smi_path(Path(configured))
    for candidate in _PLATFORM_NVIDIA_SMI_PATHS:
        executable = _validated_nvidia_smi_path(candidate)
        if executable is not None:
            return executable
    return None


def _nvidia_smi_query() -> dict[str, Any]:
    executable = _nvidia_smi_path()
    if executable is None:
        return {}

    try:
        cmd = [
            executable,
            "--query-gpu=name,memory.total,driver_version",
            "--format=csv,noheader,nounits",
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, check=False, timeout=5)
        if result.returncode != 0 or not result.stdout.strip():
            return {}
        parts = result.stdout.strip().split(", ")
        return {
            "name": parts[0],
            "vram_mb": int(float(parts[1])),
            "driver": parts[2],
        }
    except (IndexError, OSError, subprocess.SubprocessError, ValueError):
        return {}


def _ort_providers() -> list[str]:
    try:
        import onnxruntime as ort

        return ort.get_available_providers()
    except ImportError:
        return []


def _ort_version() -> str:
    try:
        import onnxruntime as ort

        return ort.__version__
    except ImportError:
        return "N/A"


def gpu_info() -> dict[str, Any]:
    smi = _nvidia_smi_query()
    providers = _ort_providers()
    return {
        "gpu_name": smi.get("name", "N/A"),
        "vram_mb": smi.get("vram_mb", 0),
        "driver": smi.get("driver", "N/A"),
        "ort_version": _ort_version(),
        "providers": providers,
        "has_cuda": "CUDAExecutionProvider" in providers,
        "has_tensorrt": "TensorrtExecutionProvider" in providers,
        "has_dml": "DmlExecutionProvider" in providers,
        "has_openvino": "OpenVINOExecutionProvider" in providers,
        "has_coreml": "CoreMLExecutionProvider" in providers,
        "platform": platform.system(),
        "arch": platform.machine(),
    }


def recommend_device() -> str:
    info = gpu_info()
    if info.get("has_tensorrt"):
        return "tensorrt"
    if info.get("has_cuda"):
        return "cuda:0"
    return "cpu"
