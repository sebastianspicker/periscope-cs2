"""Provider-specific option builders and device→provider-list resolver."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from cs2_vision_access.adapters.models.runtime.providers.detect import gpu_info


def _cuda_provider_options(
    device_id: int = 0,
    gpu_mem_limit_mb: int | None = None,
    arena_strategy: str = "kSameAsRequested",
    cudnn_algo_search: str = "HEURISTIC",
    do_copy_in_default_stream: bool = True,
    prefer_nhwc: bool = False,
) -> dict[str, Any]:
    opts: dict[str, Any] = {
        "device_id": device_id,
        "arena_extend_strategy": arena_strategy,
        "cudnn_conv_algo_search": cudnn_algo_search,
        "do_copy_in_default_stream": do_copy_in_default_stream,
        "prefer_nhwc": prefer_nhwc,
    }
    if gpu_mem_limit_mb is not None:
        opts["gpu_mem_limit"] = gpu_mem_limit_mb * 1024 * 1024
    return opts


def _tensorrt_provider_options(
    device_id: int = 0,
    fp16_enable: bool = True,
    int8_enable: bool = False,
    engine_cache_path: str | Path = "artifacts/trt_cache",
    builder_optimization_level: int = 3,
    max_workspace_size_mb: int = 1024,
) -> dict[str, Any]:
    cache = Path(engine_cache_path)
    cache.mkdir(parents=True, exist_ok=True)

    opts: dict[str, Any] = {
        "device_id": device_id,
        "trt_fp16_enable": fp16_enable,
        "trt_int8_enable": int8_enable,
        "trt_engine_cache_enable": True,
        "trt_engine_cache_path": str(cache.resolve()),
        "trt_builder_optimization_level": builder_optimization_level,
        "trt_max_workspace_size": max_workspace_size_mb * 1024 * 1024,
        "trt_dla_enable": False,
    }
    return opts


def _auto_vram_limit() -> int:
    info = gpu_info()
    total = info.get("vram_mb", 0)
    if total > 0:
        return int(total * 0.9)
    return 2048


def resolve_ort_providers(
    device: str,
    gpu_mem_limit_mb: int | None = None,
    tensorrt_cache: str | Path = "artifacts/trt_cache",
) -> list[tuple[str, dict[str, Any]] | str]:
    _CPU = "CPUExecutionProvider"

    if device == "cpu":
        return [_CPU]

    if device.startswith("cuda") or device.startswith("cuda:"):
        dev_id = 0
        if ":" in device:
            try:
                dev_id = int(device.split(":")[1])
            except (ValueError, IndexError):
                dev_id = 0
        cuda_opts = _cuda_provider_options(
            device_id=dev_id,
            gpu_mem_limit_mb=gpu_mem_limit_mb or _auto_vram_limit(),
        )
        return [("CUDAExecutionProvider", cuda_opts), _CPU]

    if device.startswith("tensorrt"):
        trt_opts = _tensorrt_provider_options(
            device_id=0,
            engine_cache_path=tensorrt_cache,
        )
        cuda_opts = _cuda_provider_options(device_id=0)
        return [("TensorrtExecutionProvider", trt_opts), ("CUDAExecutionProvider", cuda_opts), _CPU]

    if device.startswith("dml"):
        return ["DmlExecutionProvider", _CPU]

    if device.startswith("openvino"):
        return ["OpenVINOExecutionProvider", _CPU]

    if device.startswith("coreml") or (sys.platform == "darwin" and device.startswith("mps")):
        return ["CoreMLExecutionProvider", _CPU]

    return [_CPU]
