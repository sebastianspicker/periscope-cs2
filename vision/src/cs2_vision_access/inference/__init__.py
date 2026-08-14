"""Inference pipeline, temporal filter, supervision bridge, providers, optimisation."""

from cs2_vision_access.inference.cuda import (  # noqa: F401  # CUDA kernels (auto fallback)
    cuda_available,
    gpu_letterbox,
    gpu_yolo_decode,
    nms,
)
from cs2_vision_access.inference.pipeline import (
    LivePipelineConfig,
    LivePipelineError,
    LiveRunSummary,
    run_live_pipeline,
)
from cs2_vision_access.inference.providers import (
    auto_configure,
    benchmark_device,
    create_ort_session,
    gpu_info,
    recommend_device,
    resolve_ort_providers,
)
from cs2_vision_access.inference.supervision import (
    SupervisionAnnotator,
    SupervisionAnnotatorConfig,
    SupervisionBridgeError,
    detections_to_instance_masks,
    instance_masks_to_detections,
)
from cs2_vision_access.inference.temporal import (
    SuppressOnlyTemporalPolicy,
    TemporalDiagnostics,
    TemporalStabilityConfig,
)

__all__ = [
    "LivePipelineConfig",
    "LivePipelineError",
    "LiveRunSummary",
    "SuppressOnlyTemporalPolicy",
    "TemporalDiagnostics",
    "TemporalStabilityConfig",
    "SupervisionAnnotator",
    "SupervisionAnnotatorConfig",
    "SupervisionBridgeError",
    "auto_configure",
    "benchmark_device",
    "cuda_available",
    "create_ort_session",
    "detections_to_instance_masks",
    "gpu_info",
    "gpu_letterbox",
    "gpu_yolo_decode",
    "instance_masks_to_detections",
    "nms",
    "recommend_device",
    "resolve_ort_providers",
    "run_live_pipeline",
]
