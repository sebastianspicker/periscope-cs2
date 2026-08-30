"""GPU-accelerated operations with automatic CPU fallback.

Provides a complete CUDA-accelerated inference pipeline:

- **Unified pipeline** — ``GpuInferencePipeline`` chains preprocess → infer
  → decode → NMS all on GPU (with ``GpuPipelineConfig`` / ``DetectionResult``).
- **CUDA streams** — async pipeline stages (transfer → compute → inference);
  the streams are wired into the pipeline for overlapping preprocess,
  inference, and post-processing.
- **Memory pool** — pre-allocated GPU buffers for zero-allocation inference;
  used by the decode kernels and the direct GPU letterbox preprocess.
- **Per-operation kernels** — NMS, letterbox, YOLO decode, marching squares.
- **CPU fallback** — every operation works without a GPU (pure NumPy).

Usage::

    from cs2_vision_access.adapters.models.runtime.cuda import GpuInferencePipeline, nms
    keep = nms(boxes, ...)
    boxes, scores, ids = decode(raw_output, 4, 1920, 1080)
"""

from __future__ import annotations

from cs2_vision_access.adapters.models.runtime.cuda.decode import decode as decode
from cs2_vision_access.adapters.models.runtime.cuda.memory import (
    GpuMemoryPool as GpuMemoryPool,
)
from cs2_vision_access.adapters.models.runtime.cuda.memory import (
    get_pool as get_pool,
)
from cs2_vision_access.adapters.models.runtime.cuda.memory import (
    reset_pool as reset_pool,
)
from cs2_vision_access.adapters.models.runtime.cuda.nms import nms as nms
from cs2_vision_access.adapters.models.runtime.cuda.pipeline import (
    DetectionResult as DetectionResult,
)
from cs2_vision_access.adapters.models.runtime.cuda.pipeline import (
    GpuInferencePipeline as GpuInferencePipeline,
)
from cs2_vision_access.adapters.models.runtime.cuda.pipeline import (
    GpuPipelineConfig as GpuPipelineConfig,
)
from cs2_vision_access.adapters.models.runtime.cuda.preprocess import (
    gpu_letterbox as gpu_letterbox,
)
from cs2_vision_access.adapters.models.runtime.cuda.preprocess import (
    gpu_mask_to_polygon as gpu_mask_to_polygon,
)
from cs2_vision_access.adapters.models.runtime.cuda.preprocess import (
    gpu_yolo_decode as gpu_yolo_decode,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    STREAM_COMPUTE as STREAM_COMPUTE,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    STREAM_DEFAULT as STREAM_DEFAULT,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    STREAM_TRANSFER as STREAM_TRANSFER,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    PipelineSync as PipelineSync,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    get_streams as get_streams,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    on_stream as on_stream,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    sync_all as sync_all,
)
from cs2_vision_access.adapters.models.runtime.cuda.stream import (
    sync_stream as sync_stream,
)
from cs2_vision_access.adapters.models.runtime.cuda.utils import (
    cuda_available as cuda_available,
)
from cs2_vision_access.adapters.models.runtime.cuda.utils import (
    gpu_info as gpu_info,
)

__all__ = [
    "DetectionResult",
    "GpuInferencePipeline",
    "GpuMemoryPool",
    "GpuPipelineConfig",
    "PipelineSync",
    "STREAM_COMPUTE",
    "STREAM_DEFAULT",
    "STREAM_TRANSFER",
    "cuda_available",
    "decode",
    "get_pool",
    "get_streams",
    "gpu_info",
    "gpu_letterbox",
    "gpu_mask_to_polygon",
    "gpu_yolo_decode",
    "nms",
    "on_stream",
    "reset_pool",
    "sync_all",
    "sync_stream",
]
