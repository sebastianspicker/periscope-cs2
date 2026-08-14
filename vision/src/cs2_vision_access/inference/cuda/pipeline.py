"""Unified GPU-accelerated inference pipeline.

Chains all GPU operations into a single zero-alloc pipeline:

    capture → letterbox(GPU) → ONNX inference(GPU) → decode(GPU) → NMS(GPU) → result

Keeps all intermediate tensors in GPU memory until the final result is
transferred to CPU (or used by the overlay renderer on the same GPU).
"""

from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import Any

import numpy as np

from cs2_vision_access.inference.cuda.memory import GpuMemoryPool, get_pool
from cs2_vision_access.inference.cuda.pipeline_postprocess import (
    _decode_nms,
    _filter_by_confidence,
    _session_input_dtype,
)
from cs2_vision_access.inference.cuda.pipeline_preprocess_gpu import _preprocess_gpu_direct
from cs2_vision_access.inference.cuda.preprocess import gpu_letterbox
from cs2_vision_access.inference.cuda.utils import cuda_available

logger = logging.getLogger(__name__)


@dataclass
class GpuPipelineConfig:
    """Configuration for the GPU inference pipeline."""

    image_size: int = 640
    confidence: float = 0.4
    iou_threshold: float = 0.5
    num_classes: int = 4
    use_fp16: bool = True
    enable_streaming: bool = True
    prealloc_buffers: bool = True


@dataclass
class DetectionResult:
    """Result of a single inference run on GPU.

    ``boxes`` / ``scores`` / ``class_ids`` are NumPy arrays by default.
    When ``GpuInferencePipeline.run(..., return_gpu=True)`` succeeds on a
    CUDA host, they may be CuPy arrays (still indexable; call
    ``cupy.asnumpy`` if a host array is required).
    """

    boxes: Any  # [N, 4] xyxy — NumPy, or CuPy when return_gpu=True
    scores: Any  # [N]
    class_ids: Any  # [N]
    num_detections: int
    gpu_time_ms: float = 0.0
    pipeline_time_ms: float = 0.0
    on_gpu: bool = False


# ---------------------------------------------------------------------------
# GPU Pipeline
# ---------------------------------------------------------------------------


class GpuInferencePipeline:
    """GPU-accelerated chain: preprocess → infer → decode → NMS.

    Usage::

        pipeline = GpuInferencePipeline(
            ort_session=session,
            num_classes=4,
        )
        result = pipeline.run(frame_bgr)

        for box, score, cls in zip(result.boxes, result.scores, result.class_ids):
            print(f"  {cls}: {box} ({score:.2f})")
    """

    def __init__(
        self,
        ort_session: Any,
        *,
        config: GpuPipelineConfig | None = None,
        num_classes: int | None = None,
        image_size: int | None = None,
        confidence: float | None = None,
        iou_threshold: float | None = None,
        use_pool: bool | None = None,
        det_input_name: str = "images",
        det_output_name: str = "output0",
    ) -> None:
        """Initialize the GPU inference pipeline.

        Args:
            config: Optional :class:`GpuPipelineConfig`.  When provided it
                supplies defaults for ``num_classes``, ``image_size``,
                ``confidence``, ``iou_threshold``, ``use_fp16``,
                ``enable_streaming`` and ``prealloc_buffers``.  Explicit
                keyword arguments always win over config values
                (``explicit kwarg > config > class default``).
        """
        cfg = config if config is not None else GpuPipelineConfig()

        self.session = ort_session
        self.num_classes = cfg.num_classes if num_classes is None else num_classes
        self.image_size = cfg.image_size if image_size is None else image_size
        self.confidence = cfg.confidence if confidence is None else confidence
        self.iou_threshold = cfg.iou_threshold if iou_threshold is None else iou_threshold
        self.enable_streaming = cfg.enable_streaming
        self.use_fp16 = cfg.use_fp16
        self.det_input_name = det_input_name
        self.det_output_name = det_output_name
        self.cuda_available = cuda_available()

        # GPU memory pool (shared across all pipeline instances)
        use_pool = cfg.prealloc_buffers if use_pool is None else use_pool
        self.pool: GpuMemoryPool | None = get_pool() if use_pool else None

        # Timing
        self._timings: list[float] = []
        self._pipeline_timings: list[float] = []

        # Input dtype resolved from the ONNX session during IO binding.
        # Defaults to float32; the session's declared input dtype is
        # authoritative — config ``use_fp16`` never forces fp16 on a
        # float32 model (correctness first).
        self._input_dtype: np.dtype = np.dtype(np.float32)

        # Resolve I/O bindings for ONNX Runtime (GPU binding if available)
        self._io_binding = None
        if self.cuda_available:
            self._setup_io_binding()

    def _setup_io_binding(self) -> None:
        """Configure ONNX Runtime IO binding for direct GPU tensor I/O.

        This avoids the default CPU-based tensor transfer and keeps data
        on the GPU throughout the pipeline.  The bound input buffer uses the
        model's actual input element type (float32/float16) so the allocation,
        the IO-binding element type and the data written by the letterbox
        kernel all agree (no FP16/FP32 mismatch).
        """
        import cupy as cp
        import onnxruntime as ort

        step: str = ""
        try:
            # Determine the model's actual input dtype from the session.
            step = "input dtype resolution"
            self._input_dtype = _session_input_dtype(self.session)

            # Create an IO binding that uses CUDA allocator
            step = "session.io_binding()"
            binding = self.session.io_binding()

            # Input — allocate and bind with the model's actual dtype.
            step = "input binding"
            det_shape = (1, 3, self.image_size, self.image_size)
            det_input_gpu = cp.empty(det_shape, dtype=self._input_dtype)
            binding.bind_input(
                name=self.det_input_name,
                device_type="cuda",
                device_id=0,
                element_type=ort.OrtValue.ort_type_from_numpy(self._input_dtype),
                shape=tuple(det_input_gpu.shape),
                buffer_ptr=det_input_gpu.data.ptr,
            )

            # Output
            step = "output binding"
            det_output_gpu = cp.empty((1, 4 + self.num_classes, 8400), dtype=cp.float32)
            binding.bind_output(
                name=self.det_output_name,
                device_type="cuda",
                device_id=0,
                element_type=ort.OrtValue.ort_type_from_numpy(np.dtype(np.float32)),
                shape=tuple(det_output_gpu.shape),
                buffer_ptr=det_output_gpu.data.ptr,
            )

            self._io_binding = binding
            self._io_input = det_input_gpu
            self._io_output = det_output_gpu
            logger.info(
                "GPU IO binding enabled (%s input) — zero-copy inference",
                self._input_dtype,
            )
        except Exception as exc:
            logger.warning(
                "GPU IO binding failed at %s (%s: %s), falling back to CPU tensor transfer",
                step,
                type(exc).__name__,
                exc,
            )
            self._io_binding = None

    def _use_fp16(self) -> bool:
        """Whether the bound input buffer is FP16.

        Reports the input dtype resolved from the ONNX session during
        ``_setup_io_binding``.  The session's declared input dtype is
        authoritative — fp16 is never forced on a float32 model.
        """
        return bool(self._input_dtype == np.float16)

    def _streams_enabled(self) -> bool:
        """Whether async CUDA streams should be used for this run.

        Requires streaming to be enabled in the config, CUDA availability,
        and a working CuPy install.  Lazily creates the stream singleton on
        first use; any failure degrades to fully synchronous execution.
        """
        if not self.enable_streaming or not self.cuda_available:
            return False
        try:
            from cs2_vision_access.inference.cuda.stream import get_streams

            get_streams()
        except Exception:
            return False
        return True

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def run(
        self,
        frame_bgr: np.ndarray,
        *,
        return_gpu: bool = False,
    ) -> DetectionResult:
        """Run full inference pipeline on one BGR frame.

        Args:
            frame_bgr: BGR uint8 frame ``(H, W, 3)``.
            return_gpu: If ``True``, boxes/scores/class_ids remain as CuPy
                arrays on GPU (faster for further GPU processing).

        Returns:
            ``DetectionResult`` with CPU (or GPU) arrays.
        """
        import time

        t0 = time.perf_counter()

        # ---- Step 1: Preprocess (letterbox) — GPU ----
        h, w = frame_bgr.shape[:2]

        if self.cuda_available and self._io_binding is not None:
            # Direct GPU letterbox into pre-allocated input buffer.  When
            # streaming is enabled, upload + kernel run on the transfer
            # stream so they can overlap with inference of previous frames.
            if self._streams_enabled():
                try:
                    from cs2_vision_access.inference.cuda.stream import (
                        STREAM_TRANSFER,
                        on_stream,
                    )

                    with on_stream(STREAM_TRANSFER):
                        _preprocess_gpu_direct(
                            frame_bgr,
                            self._io_input,
                            self.image_size,
                            self.pool,
                        )
                except Exception as exc:
                    logger.warning("Streamed preprocess failed (%s) — running synchronously", exc)
                    _preprocess_gpu_direct(
                        frame_bgr,
                        self._io_input,
                        self.image_size,
                        self.pool,
                    )
            else:
                _preprocess_gpu_direct(
                    frame_bgr,
                    self._io_input,
                    self.image_size,
                    self.pool,
                )
        else:
            # CPU path
            input_tensor, scale_x, scale_y, pad_l, pad_t = gpu_letterbox(
                frame_bgr,
                self.image_size,
            )

        t1 = time.perf_counter()

        # ---- Step 2: ONNX Inference — GPU ----
        if self._io_binding is not None:
            self.session.run_with_iobinding(self._io_binding)
            raw_output = self._io_output  # already on GPU
        else:
            if self.cuda_available:
                import cupy as cp

                raw_output = cp.asarray(
                    self.session.run(
                        [self.det_output_name],
                        {self.det_input_name: input_tensor},
                    )[0]
                )
            else:
                raw_output = self.session.run(
                    [self.det_output_name],
                    {self.det_input_name: input_tensor},
                )[0]

        t2 = time.perf_counter()

        # ---- Step 3: Decode + NMS — GPU auto-dispatch ----
        # Runs on the compute stream when streaming is enabled so it can
        # overlap with the next frame's preprocessing / inference.
        if self._streams_enabled():
            try:
                from cs2_vision_access.inference.cuda.stream import (
                    STREAM_COMPUTE,
                    on_stream,
                )

                with on_stream(STREAM_COMPUTE):
                    boxes, scores, class_ids = _decode_nms(
                        raw_output,
                        self.num_classes,
                        w,
                        h,
                        self.image_size,
                        self.confidence,
                        self.iou_threshold,
                    )
            except Exception as exc:
                logger.warning("Streamed decode/NMS failed (%s) — running synchronously", exc)
                boxes, scores, class_ids = _decode_nms(
                    raw_output,
                    self.num_classes,
                    w,
                    h,
                    self.image_size,
                    self.confidence,
                    self.iou_threshold,
                )
        else:
            boxes, scores, class_ids = _decode_nms(
                raw_output,
                self.num_classes,
                w,
                h,
                self.image_size,
                self.confidence,
                self.iou_threshold,
            )

        t4 = time.perf_counter()

        # ---- Step 4: Synchronise streams before any host reads ----
        if self._streams_enabled():
            try:
                from cs2_vision_access.inference.cuda.stream import sync_all

                sync_all()
            except Exception as exc:
                logger.warning("CUDA stream synchronisation failed (%s) — continuing", exc)

        # ---- Convert to CPU unless caller asked to keep GPU arrays ----
        stay_on_gpu = bool(return_gpu and self.cuda_available)
        if stay_on_gpu:
            # Prefer real CuPy arrays when intermediates already live on device.
            try:
                import cupy as cp

                if not hasattr(boxes, "__cuda_array_interface__"):
                    boxes = cp.asarray(boxes, dtype=cp.float32)
                    scores = cp.asarray(scores, dtype=cp.float32)
                    class_ids = cp.asarray(class_ids, dtype=cp.int32)
            except Exception:
                stay_on_gpu = False
                boxes = np.asarray(boxes, dtype=np.float32)
                scores = np.asarray(scores, dtype=np.float32)
                class_ids = np.asarray(class_ids, dtype=np.int32)
        else:
            if self.cuda_available:
                import cupy as cp

                boxes = cp.asnumpy(boxes) if hasattr(boxes, "get") else boxes
                scores = cp.asnumpy(scores) if hasattr(scores, "get") else scores
                class_ids = cp.asnumpy(class_ids) if hasattr(class_ids, "get") else class_ids
            boxes = np.asarray(boxes, dtype=np.float32)
            scores = np.asarray(scores, dtype=np.float32)
            class_ids = np.asarray(class_ids, dtype=np.int32)

        n_det = int(len(scores)) if not hasattr(scores, "shape") else int(scores.shape[0])
        result = DetectionResult(
            boxes=boxes,
            scores=scores,
            class_ids=class_ids,
            num_detections=n_det,
            gpu_time_ms=(t2 - t1) * 1000,
            pipeline_time_ms=(t4 - t0) * 1000,
            on_gpu=stay_on_gpu,
        )

        self._timings.append(result.gpu_time_ms)
        self._pipeline_timings.append(result.pipeline_time_ms)
        return result

    # ------------------------------------------------------------------
    # Timing
    # ------------------------------------------------------------------

    @property
    def avg_gpu_ms(self) -> float:
        """Average GPU inference time over recent frames."""
        recent = self._timings[-100:]
        return sum(recent) / len(recent) if recent else 0.0

    @property
    def avg_pipeline_ms(self) -> float:
        """Average end-to-end pipeline time over recent frames."""
        recent = self._pipeline_timings[-100:]
        return sum(recent) / len(recent) if recent else 0.0


__all__ = [
    "DetectionResult",
    "GpuInferencePipeline",
    "GpuPipelineConfig",
    "_filter_by_confidence",
]
