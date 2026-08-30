"""Model-independent segmentation port."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Protocol

import numpy as np

from cs2_vision_access.domain.predictions import InstanceMask


class Segmenter(Protocol):
    """Predict current-frame masks without retaining external I/O state."""

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]: ...


DEFAULT_SEGMENTER_BACKEND = "ultralytics-onnx"
RFDETR_BACKEND = "rfdetr"
ULTRALYTICS_DETECT_BACKEND = "ultralytics-detect"
YOLOV10_BACKEND = "yolov10"
NANODET_BACKEND = "nanodet"
CS2_SAM_BACKEND = "cs2-sam"
SUPPORTED_SEGMENTER_BACKENDS = frozenset(
    {
        DEFAULT_SEGMENTER_BACKEND,
        RFDETR_BACKEND,
        ULTRALYTICS_DETECT_BACKEND,
        YOLOV10_BACKEND,
        NANODET_BACKEND,
        CS2_SAM_BACKEND,
    }
)
_BACKEND_ALIASES = {
    "rf-detr": RFDETR_BACKEND,
    "yolo-detect": ULTRALYTICS_DETECT_BACKEND,
    "yolov10n": YOLOV10_BACKEND,
    "yolov10-nano": YOLOV10_BACKEND,
    "nanodet-plus": NANODET_BACKEND,
    "nanodet-plus-m": NANODET_BACKEND,
    "cs2_sam": CS2_SAM_BACKEND,
    "cs2sam": CS2_SAM_BACKEND,
    "vombit-sam": CS2_SAM_BACKEND,
    "cs2-edge-sam": CS2_SAM_BACKEND,
}


def normalize_segmenter_backend(backend: str) -> str:
    """Normalize a backend name without loading a model implementation."""
    normalized = backend.strip().casefold()
    return _BACKEND_ALIASES.get(normalized, normalized)


class SegmenterFactory(Protocol):
    def __call__(
        self, model_path: str | Path, manifest_path: str | Path, **kwargs: Any
    ) -> Segmenter: ...


_factory: SegmenterFactory | None = None


def register_segmenter_factory(factory: SegmenterFactory) -> None:
    """Install the concrete factory at an outer composition boundary."""
    global _factory
    _factory = factory


def create_segmenter(model_path: str | Path, manifest_path: str | Path, **kwargs: Any) -> Segmenter:
    """Create through the registered adapter; workflows never select adapters."""
    if _factory is None:
        raise RuntimeError(
            "no segmenter factory is registered; compose an adapter at the interface"
        )
    return _factory(Path(model_path), Path(manifest_path), **kwargs)


def Cs2SamSegmenter(model_path: str | Path, manifest_path: str | Path, **kwargs: Any) -> Segmenter:
    """Compatibility constructor for workflow composition using the cs2-sam backend."""
    return create_segmenter(model_path, manifest_path, backend=CS2_SAM_BACKEND, **kwargs)
