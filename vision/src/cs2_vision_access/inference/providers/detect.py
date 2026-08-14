"""Device capability detection — GPU info, VRAM, driver, ORT providers."""

from __future__ import annotations

import platform
from typing import Any


def _nvidia_smi_query() -> dict[str, Any]:
    import shlex
    import subprocess

    try:
        cmd = shlex.split(
            "nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader,nounits"
        )
        result = subprocess.run(cmd, capture_output=True, text=True, check=False, timeout=5)
        if result.returncode != 0 or not result.stdout.strip():
            return {}
        parts = result.stdout.strip().split(", ")
        return {
            "name": parts[0],
            "vram_mb": int(float(parts[1])),
            "driver": parts[2],
        }
    except Exception:
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
