"""Documented compatibility facade for live inference."""

from cs2_vision_access.adapters.models.runtime.cuda import (  # noqa: F401
    cuda_available,
    gpu_letterbox,
    gpu_yolo_decode,
    nms,
)
from cs2_vision_access.adapters.models.runtime.providers import (  # noqa: F401
    auto_configure,
    benchmark_device,
    create_ort_session,
    gpu_info,
    recommend_device,
    resolve_ort_providers,
)
from cs2_vision_access.application.live.inference.temporal import (  # noqa: F401
    SuppressOnlyTemporalPolicy,
    TemporalDiagnostics,
    TemporalStabilityConfig,
)
