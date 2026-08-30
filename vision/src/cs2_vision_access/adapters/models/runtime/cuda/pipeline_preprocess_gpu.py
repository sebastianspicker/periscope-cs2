"""Direct GPU letterbox preprocess into a pre-allocated input buffer."""

from __future__ import annotations

import logging
from typing import Any

import numpy as np

from cs2_vision_access.adapters.models.runtime.cuda.memory import GpuMemoryPool

logger = logging.getLogger(__name__)


def _preprocess_gpu_direct(
    frame_bgr: np.ndarray,
    dst_buffer: Any,
    target_size: int,
    pool: GpuMemoryPool | None,
) -> None:
    """Upload a BGR frame directly into the GPU input buffer with letterbox.

    The letterbox kernel always writes ``float32``; when the bound input
    buffer is ``float16`` (an FP16 model), the kernel result is first staged
    in a temporary float32 buffer and then down-cast on device.
    """
    import cupy as cp

    h, w = frame_bgr.shape[:2]
    scale = target_size / max(h, w)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))
    pad_left = (target_size - new_w) // 2
    pad_top = (target_size - new_h) // 2

    # Upload frame to GPU — pinned-memory fast path with plain fallback.
    src_gpu: Any = None
    if pool is not None and frame_bgr.nbytes > 0:
        try:
            pinned = pool.pinned_bytes(frame_bgr.nbytes)
            if pinned is not None:
                pinned_view = memoryview(pinned)
                pinned_view[:] = frame_bgr.tobytes()
                src_gpu = cp.asarray(
                    np.frombuffer(pinned_view, dtype=np.uint8).reshape(frame_bgr.shape)
                )
        except Exception as exc:
            logger.warning("Pinned-memory upload failed (%s) — using plain upload", exc)
            src_gpu = None
    if src_gpu is None:
        src_gpu = cp.asarray(frame_bgr)

    # Run letterbox kernel into a float32 destination (kernel writes float32).
    from cs2_vision_access.adapters.models.runtime.cuda.kernels import LETTERBOX_KERNEL_CU

    kernel = cp.RawKernel(LETTERBOX_KERNEL_CU, "letterbox_kernel")
    threads = (16, 16, 1)
    blocks = (
        (target_size + 15) // 16,
        (target_size + 15) // 16,
        3,
    )

    tmp: Any = None
    if dst_buffer.dtype == cp.float16:
        tmp = cp.empty_like(dst_buffer, dtype=cp.float32)

    kernel(
        blocks,
        threads,
        (
            src_gpu,
            w,
            h,
            frame_bgr.strides[0],
            tmp if tmp is not None else dst_buffer,
            target_size,
            target_size,
            cp.float32(w / new_w),
            cp.float32(h / new_h),
            pad_left,
            pad_top,
        ),
    )

    if tmp is not None:
        dst_buffer[...] = tmp.astype(cp.float16)
