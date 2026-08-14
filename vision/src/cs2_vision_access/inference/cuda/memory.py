"""GPU memory pool — pre-allocated buffers for zero-allocation inference.

Repeated ``cp.zeros(...)`` calls cause GPU memory fragmentation and allocation
overhead.  This module pre-allocates fixed-size buffers for the common tensor
shapes used in the inference pipeline and reuses them across frames.

The pool is a singleton: first call to ``get_pool()`` creates it, subsequent
calls return the same instance.  All pool members are CuPy arrays that live
for the entire process lifetime.
"""

from __future__ import annotations

import logging
from typing import Any

from cs2_vision_access.inference.cuda.utils import cuda_available

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Pool singleton
# ---------------------------------------------------------------------------

_POOL: GpuMemoryPool | None = None


def get_pool() -> GpuMemoryPool | None:
    """Return the global GPU memory pool, or ``None`` if CUDA is unavailable."""
    global _POOL  # noqa: PLW0603
    if _POOL is None and cuda_available():
        _POOL = GpuMemoryPool()
        logger.info("GPU memory pool created (%d buffers pre-allocated)", len(_POOL._buffers))
    return _POOL


def reset_pool() -> None:
    """Destroy the global pool (forces re-allocation on next ``get_pool()``)."""
    global _POOL  # noqa: PLW0603
    _POOL = None


# ---------------------------------------------------------------------------
# Pool implementation
# ---------------------------------------------------------------------------


class GpuMemoryPool:
    """Pre-allocated GPU buffers for common inference tensor shapes.

    Buffers are indexed by a string key (e.g. ``"det_input_1x3x640x640"``)
    and exposed as properties.  Callers may mutate the buffer contents freely
    but must not resize them.
    """

    def __init__(self, base_image_size: int = 640) -> None:
        import cupy as cp

        self._cp = cp
        self._buffers: dict[str, cp.ndarray] = {}
        self._base_size = base_image_size

        # ---- Input buffers ----
        # Vombit detection: [1, 3, 416/640, 416/640] float32
        self._prealloc("det_input", 1, 3, base_image_size, base_image_size)

        # EdgeSAM encoder: [1, 3, 1024, 1024] float32 (ImageNet-normalised)
        self._prealloc("sam_input", 1, 3, 1024, 1024)

        # ---- Output buffers ----
        # Vombit raw output: [1, 88, 8400] (4 + 84 classes, 8400 predictions)
        self._prealloc("det_output", 1, 88, 8400)

        # EdgeSAM encoder output: [1, 256, 64, 64] image embeddings
        self._prealloc("sam_embeddings", 1, 256, 64, 64)

        # EdgeSAM decoder mask input (fixed): [1, 1, 256, 256]
        self._prealloc("sam_mask_input", 1, 1, 256, 256)

        # EdgeSAM decoder mask output (upscaled): [1, 1, 1080, 1920] max
        self._prealloc("sam_mask_output", 1, 1, 1080, 1920)

        # ---- Decode buffers ----
        # Boxes [8400, 4], Scores [8400], ClassIDs [8400]
        self._prealloc("boxes", 8400, 4)
        self._prealloc("scores", 8400)
        # Class IDs are used as an index into the class-probability block, so
        # they must be an integer dtype (a float32 index raises on CuPy and
        # silently downgraded GPU decode to CPU on every frame).
        self._prealloc("class_ids", 8400, dtype=cp.int32)

        # ---- NMS buffers ----
        self._prealloc("nms_suppressed", 8400, dtype=cp.int32)

        # ---- Recycle host-side pinned memory for fast CPU↔GPU transfer ----
        self._host_pinned = cp.cuda.alloc_pinned_memory(
            1920 * 1080 * 3  # max frame size (BGR)
        )

    # -- Properties ---------------------------------------------------------

    @property
    def det_input(self) -> Any:
        return self._buffers[f"det_input_1x3x{self._base_size}x{self._base_size}"]

    @property
    def sam_input(self) -> Any:
        return self._buffers["sam_input_1x3x1024x1024"]

    @property
    def det_output(self) -> Any:
        return self._buffers["det_output_1x88x8400"]

    @property
    def sam_embeddings(self) -> Any:
        return self._buffers["sam_embeddings_1x256x64x64"]

    @property
    def sam_mask_input(self) -> Any:
        return self._buffers["sam_mask_input_1x1x256x256"]

    @property
    def sam_mask_output(self) -> Any:
        return self._buffers["sam_mask_output_1x1x1080x1920"]

    @property
    def boxes(self) -> Any:
        return self._buffers["boxes_8400x4"]

    @property
    def scores(self) -> Any:
        return self._buffers["scores_8400"]

    @property
    def class_ids(self) -> Any:
        return self._buffers["class_ids_8400"]

    @property
    def nms_suppressed(self) -> Any:
        return self._buffers["nms_suppressed_8400"]

    @property
    def pinned_host(self) -> memoryview:
        return self._host_pinned

    def pinned_bytes(self, n: int) -> memoryview | None:
        """Return a writable ``memoryview`` over the first ``n`` pinned bytes.

        Returns ``None`` when the pinned host buffer is unavailable or ``n``
        exceeds its capacity (callers should fall back to a plain upload).
        """
        try:
            if n < 0 or n > len(self._host_pinned):
                return None
            return self._cp.cuda.memory.memoryview(self._host_pinned)[:n]
        except Exception:
            return None

    # -- Buffer management --------------------------------------------------

    def _prealloc(self, name: str, *shape: int, dtype: Any = None) -> None:
        """Pre-allocate a GPU buffer with the given shape.

        Args:
            name: Buffer key prefix (e.g. ``"boxes"``).
            shape: Buffer dimensions (e.g. ``(8400, 4)``).
            dtype: CuPy dtype; defaults to ``float32``.
        """
        key = f"{name}_{'x'.join(str(s) for s in shape)}"
        if key not in self._buffers:
            buf = self._cp.empty(shape, dtype=dtype or self._cp.float32)
            buf[:] = 0
            self._buffers[key] = buf

    def slice_for(self, name: str, n: int, dim: int | None = None) -> Any:
        """Return a slice of a pre-allocated buffer limited to ``n`` rows.

        Args:
            name: Buffer key prefix (e.g. ``"boxes"``).
            n: Number of rows to use.
            dim: Optional second dimension.

        Returns:
            A view of the underlying buffer truncated to ``n`` elements.
        """
        key = f"{name}_{'x'.join(str(s) for s in [n, dim] if dim)}" if dim else f"{name}_{n}"
        # If exact size exists, return it
        if key in self._buffers:
            return self._buffers[key]

        # Prefer a pre-allocated 8400-slot buffer (e.g. ``boxes_8400x4`` or
        # ``scores_8400``) and slice to n rows.  First match in insertion
        # order; buffers are pre-allocated once, so this is deterministic.
        prefix = f"{name}_8400"
        buf = next(
            (self._buffers[key] for key in self._buffers if key.startswith(prefix)),
            None,
        )
        if buf is not None:
            return buf[:n]
        return None

    def __len__(self) -> int:
        return len(self._buffers)

    def __repr__(self) -> str:
        total_mb = sum(buf.nbytes for buf in self._buffers.values()) / (1024 * 1024)
        return f"<GpuMemoryPool {len(self._buffers)} buffers, {total_mb:.1f} MB>"


__all__ = ["GpuMemoryPool", "get_pool", "reset_pool"]
