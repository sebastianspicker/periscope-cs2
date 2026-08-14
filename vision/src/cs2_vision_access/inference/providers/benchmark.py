"""Device benchmarking and auto-configuration."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np

from cs2_vision_access.inference.providers.detect import gpu_info
from cs2_vision_access.inference.providers.session import create_ort_session


def benchmark_device(
    model_path: str | Path,
    device: str = "cpu",
    n_warmup: int = 10,
    n_bench: int = 50,
    input_size: tuple[int, int] = (640, 640),
    batch: int = 1,
) -> dict[str, Any]:
    import time

    session = create_ort_session(model_path, device=device)

    inp = session.get_inputs()[0]
    shape = inp.shape
    if len(shape) == 4:
        n, c, h, w = shape
        if h is None:
            h = input_size[0]
        if w is None:
            w = input_size[1]
    else:
        h, w = input_size
        c = 3
        n = batch

    dummy = np.random.randn(n, c, h, w).astype(np.float32)
    input_name = inp.name
    output_name = session.get_outputs()[0].name

    for _ in range(n_warmup):
        session.run([output_name], {input_name: dummy})

    timings: list[float] = []
    for _ in range(n_bench):
        start = time.perf_counter()
        session.run([output_name], {input_name: dummy})
        timings.append((time.perf_counter() - start) * 1000.0)

    timings.sort()
    mean_ms = sum(timings) / len(timings)
    p50 = timings[len(timings) // 2]
    p95 = timings[int(len(timings) * 0.95)]
    fps = 1000.0 / mean_ms if mean_ms > 0 else 0.0

    return {
        "device": device,
        "mean_ms": round(mean_ms, 2),
        "p50_ms": round(p50, 2),
        "p95_ms": round(p95, 2),
        "min_ms": round(timings[0], 2),
        "max_ms": round(timings[-1], 2),
        "fps": round(fps, 1),
    }


def auto_configure(
    model_path: str | Path,
    prefer_device: str | None = None,
) -> dict[str, Any]:
    info = gpu_info()

    candidates = ["tensorrt", "cuda:0", "dml", "cpu"]
    if prefer_device and prefer_device in candidates:
        candidates.remove(prefer_device)
        candidates.insert(0, prefer_device)

    best = {"device": "cpu"}
    best_fps = 0.0

    for candidate in candidates:
        if candidate == "tensorrt" and not info["has_tensorrt"]:
            continue
        if candidate == "cuda:0" and not info["has_cuda"]:
            continue
        if candidate == "dml" and not info["has_dml"]:
            continue

        try:
            result = benchmark_device(model_path, device=candidate, n_warmup=5, n_bench=20)
            if result["fps"] > best_fps:
                best = {"device": candidate}
                best_fps = result["fps"]
        except Exception:
            continue

    return best
