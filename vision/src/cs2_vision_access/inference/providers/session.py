"""ONNX Runtime session creation — SessionOptions tuning and session factory."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

from cs2_vision_access.inference.providers.options import resolve_ort_providers


def _optimal_thread_count() -> int:
    count = os.cpu_count() or 4
    return min(count, 6)


def _session_options(
    *,
    device: str,
    enable_cpu_mem_arena: bool = False,
    enable_mem_reuse: bool = True,
    graph_optimization: int = 2,
) -> Any:
    import onnxruntime as ort

    opts = ort.SessionOptions()
    opts.graph_optimization_level = graph_optimization
    opts.enable_mem_reuse = enable_mem_reuse
    opts.enable_cpu_mem_arena = enable_cpu_mem_arena
    opts.intra_op_num_threads = _optimal_thread_count()
    opts.inter_op_num_threads = 1
    opts.log_severity_level = 3
    opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
    return opts


def create_ort_session(
    model_path: str | Path,
    device: str = "cpu",
    *,
    gpu_mem_limit_mb: int | None = None,
    tensorrt_cache: str | Path = "artifacts/trt_cache",
    enable_cpu_mem_arena: bool = False,
    enable_mem_reuse: bool = True,
    graph_optimization: int = 2,
) -> Any:
    import onnxruntime as ort

    opts = _session_options(
        device=device,
        enable_cpu_mem_arena=enable_cpu_mem_arena,
        enable_mem_reuse=enable_mem_reuse,
        graph_optimization=graph_optimization,
    )
    providers = resolve_ort_providers(
        device=device,
        gpu_mem_limit_mb=gpu_mem_limit_mb,
        tensorrt_cache=tensorrt_cache,
    )

    if device.startswith("cuda") or device.startswith("tensorrt"):
        opts.enable_cuda_graph = True
        opts.cuda_graph_id = hash(str(model_path)) & 0x7FFFFFFF

    try:
        session = ort.InferenceSession(
            str(model_path),
            sess_options=opts,
            providers=providers,
        )
    except Exception as error:
        if device != "cpu":
            import warnings

            warnings.warn(
                f"Failed to create GPU session with device={device!r}: {error}.  "
                "Falling back to CPU.",
                RuntimeWarning,
                stacklevel=2,
            )
            session = ort.InferenceSession(
                str(model_path),
                sess_options=opts,
                providers=["CPUExecutionProvider"],
            )
        else:
            raise

    return session
