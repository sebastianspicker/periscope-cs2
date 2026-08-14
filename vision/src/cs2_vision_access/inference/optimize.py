"""ONNX model optimisation utilities — FP16 conversion, graph optimisation, input sizing.

All operations are idempotent and produce new files alongside the originals.
Original models are never modified in place.

Typical workflow::

    from cs2_vision_access.inference.optimize import optimize_for_gpu

    # Convert a model to FP16 and apply graph optimisation
    optimized = optimize_for_gpu("artifacts/model.onnx")
    print(f"Optimised model saved to {optimized}")
"""

from __future__ import annotations

import logging
from pathlib import Path

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# FP16 conversion
# ---------------------------------------------------------------------------


def convert_to_fp16(
    onnx_path: str | Path,
    output_path: str | Path | None = None,
    *,
    keep_io_types: bool = True,
    min_positive_val: float = 1e-7,
    max_finite_val: float = 1e4,
) -> Path:
    """Convert an FP32 ONNX model to FP16.

    FP16 inference uses **half the VRAM** and runs **~1.5-2× faster** on
    NVIDIA GPUs with Tensor Cores (Volta+, all RTX cards).  Accuracy loss
    for segmentation models is typically < 0.5%.

    Args:
        onnx_path: Path to the FP32 ``.onnx`` model.
        output_path: Destination path.  Defaults to ``<stem>-fp16.onnx``
            in the same directory.
        keep_io_types: When ``True``, graph inputs/outputs remain FP32 and
            only internal layers are converted.  This avoids breaking
            framework code that expects FP32 inputs.
        min_positive_val: Clamp minimum positive value to avoid underflow.
        max_finite_val: Clamp maximum value to avoid overflow.

    Returns:
        Path to the FP16 model.
    """
    try:
        import onnx
        from onnxconverter_common import float16
    except ImportError as error:
        raise ImportError(
            "FP16 conversion requires onnx and onnxconverter-common; "
            "install with: pip install onnx onnxconverter-common"
        ) from error

    onnx_path = Path(onnx_path)
    output_path = (
        onnx_path.parent / f"{onnx_path.stem}-fp16.onnx"
        if output_path is None
        else Path(output_path)
    )

    if output_path.is_file():
        logger.info("FP16 model already exists at %s — skipping conversion", output_path)
        return output_path

    logger.info("Converting %s to FP16 → %s", onnx_path, output_path)
    model = onnx.load(str(onnx_path))
    model_fp16 = float16.convert_float_to_float16(
        model,
        keep_io_types=keep_io_types,
        min_positive_val=min_positive_val,
        max_finite_val=max_finite_val,
    )
    # Validate
    onnx.checker.check_model(model_fp16, full_check=True)
    onnx.save(model_fp16, str(output_path))

    orig_mb = onnx_path.stat().st_size / 1e6
    fp16_mb = output_path.stat().st_size / 1e6
    logger.info(
        "FP16 conversion complete: %.1f MB → %.1f MB (%.0f%% size)",
        orig_mb,
        fp16_mb,
        fp16_mb / orig_mb * 100,
    )

    return output_path


# ---------------------------------------------------------------------------
# ONNX → ORT format (graph optimisation)
# ---------------------------------------------------------------------------


def optimize_onnx_model(
    onnx_path: str | Path,
    output_path: str | Path | None = None,
    *,
    optimization_level: int = 99,  # All
    allow_conversion_to_fp16: bool = False,
) -> Path:
    """Apply ONNX Runtime graph optimisation and save as an ORT-format model.

    ORT-format models load faster and have fused graph optimisations baked in.
    They require the same ONNX Runtime version to load.

    Args:
        onnx_path: Path to the ``.onnx`` model.
        output_path: Destination path.  Defaults to ``<stem>.ort`` in the
            same directory.
        optimization_level: ORT graph optimisation level (1 = basic,
            2 = extended, 99 = all).
        allow_conversion_to_fp16: If ``True``, the optimiser may insert
            FP16 conversion nodes when the target EP supports it.

    Returns:
        Path to the optimised ``.ort`` model.
    """
    import onnxruntime as ort

    onnx_path = Path(onnx_path)
    output_path = onnx_path.with_suffix(".ort") if output_path is None else Path(output_path)

    if output_path.is_file():
        logger.info("Optimized model already exists at %s — skipping", output_path)
        return output_path

    logger.info("Optimizing %s → %s (level=%s)", onnx_path, output_path, optimization_level)

    opts = ort.SessionOptions()
    opts.graph_optimization_level = optimization_level
    opts.optimized_model_filepath = str(output_path)
    opts.log_severity_level = 3

    # Load + optimise (providers don't matter for graph optimisation itself)
    _ = ort.InferenceSession(str(onnx_path), sess_options=opts, providers=["CPUExecutionProvider"])

    optimised_mb = output_path.stat().st_size / 1e6
    logger.info("Optimization complete: %.1f MB", optimised_mb)
    return output_path


# ---------------------------------------------------------------------------
# Combined: prepare a model for GPU deployment
# ---------------------------------------------------------------------------


def optimize_for_gpu(
    onnx_path: str | Path,
    *,
    convert_fp16: bool = True,
    build_ort: bool = True,
    keep_io_types: bool = True,
    output_dir: str | Path | None = None,
) -> dict[str, Path]:
    """Run all GPU optimisations on a model in one call.

    Produces up to three files alongside the original:

    - ``<stem>-fp16.onnx`` — FP16-converted model.
    - ``<stem>.ort`` — Graph-optimised ORT-format model (from original).
    - ``<stem>-fp16.ort`` — Graph-optimised ORT-format model (from FP16).

    Args:
        onnx_path: Source FP32 ONNX model.
        convert_fp16: Whether to produce a FP16 variant.
        build_ort: Whether to produce ORT-format optimised models.
        keep_io_types: Keep FP32 inputs/outputs in FP16 variant.
        output_dir: Output directory (default: same as source).

    Returns:
        ``{"fp16": Path, "ort": Path, "fp16_ort": Path}`` — only includes
        keys for files that were produced.
    """
    src = Path(onnx_path)
    out = Path(output_dir) if output_dir else src.parent
    out.mkdir(parents=True, exist_ok=True)

    results: dict[str, Path] = {}

    if convert_fp16:
        fp16_path = convert_to_fp16(src, out / f"{src.stem}-fp16.onnx", keep_io_types=keep_io_types)
        results["fp16"] = fp16_path

    if build_ort:
        ort_path = optimize_onnx_model(src, out / f"{src.stem}.ort")
        results["ort"] = ort_path

    if convert_fp16 and build_ort:
        fp16_ort_path = optimize_onnx_model(
            results["fp16"],
            out / f"{src.stem}-fp16.ort",
        )
        results["fp16_ort"] = fp16_ort_path

    return results


# ---------------------------------------------------------------------------
# Input size suggestion (GPU-memory-aware)
# ---------------------------------------------------------------------------

# Baseline 640px throughput (fps) for the reference ~10 MB model per device.
_BASELINE_FPS: dict[str, float] = {"cpu": 6.0, "cuda": 30.0, "tensorrt": 60.0}
# Reference model size the baseline table was measured with (MB).
_REF_PARAMS_MB = 10.0
# Floor for ``model_params_mb`` so the params ratio stays finite (MB).
_MIN_PARAMS_MB = 1e-3
# Params exponent k: cost scales sub-linearly with params (0.5) because
# optimised kernels amortise per-parameter overhead (fusion, vectorisation),
# so a larger model costs less than its raw byte count suggests.
_PARAMS_EXPONENT = 0.5


def suggest_input_size(
    *,
    target_fps: float = 30.0,
    device: str = "cpu",
    model_params_mb: float = 10.0,
    measured_fps_640: float | None = None,
) -> int:
    """Suggest an input size for a given performance target.

    Returns the largest input size (multiple of 32) expected to achieve at
    least ``target_fps`` on ``device``.  Larger input sizes give better
    detection quality, especially for small/distant players.

    The estimate is a **heuristic** unless ``measured_fps_640`` is given:
    the 640px fps comes from a per-device table and is scaled by
    ``(params_ref / params) ** 0.5 * (640 / size) ** 2`` (inference cost is
    roughly proportional to pixels × params).  When ``measured_fps_640`` is
    provided it replaces the table as the 640px anchor, so the curve is
    based on a real measurement of the actual model/device.

    Rough expectations per device (YOLO11n-seg, ~10 MB params):

    ==========  =======  =======  =======
    Input size    CPU     CUDA     TensorRT
    ==========  =======  =======  =======
         320      18 fps   100+    200+ fps
         416      12 fps    70+    140+ fps
         640       6 fps    30+     60+ fps
         768       4 fps    20+     40+ fps
    ==========  =======  =======  =======

    Args:
        target_fps: Minimum desired throughput in frames per second.
            Must be positive.
        device: Target device (``"cpu"``, ``"cuda:0"``, ``"tensorrt"``).
            Unknown devices fall back to the CPU baseline.
        model_params_mb: Approximate model size in MB.  Non-positive values
            fall back to the reference ``10.0``; smaller positive values are
            clamped to a floor to keep the estimate finite.
        measured_fps_640: Optional measured 640px fps of the actual
            model/device.  Must be positive when provided.  Overrides the
            per-device table as the 640px reference point.

    Returns:
        Suggested input size (multiple of 32).
    """
    if target_fps <= 0:
        raise ValueError(f"target_fps must be positive, got {target_fps}")
    if measured_fps_640 is not None and measured_fps_640 <= 0:
        raise ValueError(f"measured_fps_640 must be positive, got {measured_fps_640}")

    # Non-positive sizes are not meaningful; fall back to the reference model.
    if model_params_mb <= 0:
        model_params_mb = _REF_PARAMS_MB
    model_params_mb = max(model_params_mb, _MIN_PARAMS_MB)

    device_key = device.split(":")[0] if ":" in device else device
    base = (
        measured_fps_640
        if measured_fps_640 is not None
        else _BASELINE_FPS.get(device_key, _BASELINE_FPS["cpu"])
    )

    # FPS scales with pixels and params: est_fps = base * (ref/params)**k * (640/size)**2
    params_scale = (_REF_PARAMS_MB / model_params_mb) ** _PARAMS_EXPONENT

    # Available sizes (multiples of 32)
    sizes = [320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768]

    # FPS scales inversely with pixels (roughly); keep the largest passing size.
    best = 320
    for size in sizes:
        est_fps = base * params_scale * (640 / size) ** 2
        if est_fps >= target_fps:
            best = size

    return best


# ---------------------------------------------------------------------------
# Module exports
# ---------------------------------------------------------------------------

__all__ = [
    "convert_to_fp16",
    "optimize_for_gpu",
    "optimize_onnx_model",
    "suggest_input_size",
]
