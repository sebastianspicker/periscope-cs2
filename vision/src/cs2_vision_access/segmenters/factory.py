"""Segmenter factory — constructs a segmenter for the named backend.

Separated from ``protocol.py`` so that adapter imports do not create cycles:
the factory imports adapters; adapters import types from ``protocol`` only.
"""

from __future__ import annotations

from pathlib import Path

from cs2_vision_access.segmenters.protocol import (
    CS2_SAM_BACKEND,
    DEFAULT_SEGMENTER_BACKEND,
    NANODET_BACKEND,
    RFDETR_BACKEND,
    SUPPORTED_SEGMENTER_BACKENDS,
    ULTRALYTICS_DETECT_BACKEND,
    YOLOV10_BACKEND,
    Segmenter,
    normalize_segmenter_backend,
)

# Built-in constructor (no lazy import needed).
from cs2_vision_access.segmenters.ultralytics import UltralyticsOnnxSegmenter  # noqa: E402

_SEGMENTER_CONSTRUCTORS: dict[str, type] = {
    DEFAULT_SEGMENTER_BACKEND: UltralyticsOnnxSegmenter,
}


def _constructor_for(backend: str) -> type:
    if backend == RFDETR_BACKEND:
        from cs2_vision_access.segmenters.rfdetr import RfDetrSegmenter

        return RfDetrSegmenter
    if backend == ULTRALYTICS_DETECT_BACKEND:
        from cs2_vision_access.segmenters.detect import UltralyticsOnnxDetector

        return UltralyticsOnnxDetector
    if backend == YOLOV10_BACKEND:
        from cs2_vision_access.segmenters.yolov10 import YoloV10Segmenter

        return YoloV10Segmenter
    if backend == NANODET_BACKEND:
        from cs2_vision_access.segmenters.nanodet import NanoDetSegmenter

        return NanoDetSegmenter
    if backend == CS2_SAM_BACKEND:
        from cs2_vision_access.segmenters.cs2_sam import Cs2SamSegmenter

        return Cs2SamSegmenter
    constructor = _SEGMENTER_CONSTRUCTORS.get(backend)
    if constructor is None:
        supported = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
        raise ValueError(
            f"unknown or unsupported segmenter backend {backend!r}; supported: {supported}"
        )
    return constructor


def create_segmenter(
    model_path: str | Path,
    manifest_path: str | Path,
    *,
    backend: str = DEFAULT_SEGMENTER_BACKEND,
    class_names: tuple[str, ...] | None = None,
    confidence: float = 0.45,
    image_size: int = 640,
    device: str = "cpu",
) -> Segmenter:
    """Construct a Segmenter for the named backend.

    The default backend is ``ultralytics-onnx`` (the built-in
    :class:`~cs2_vision_access.segmenters.ultralytics.UltralyticsOnnxSegmenter`).
    Backend names are matched after ``strip()`` and ``casefold()``.

    Optional backends (require extra dependencies):
    - ``rfdetr`` (alias ``rf-detr``) — requires ``pip install 'cs2-vision-access[rfdetr]'``
    - ``ultralytics-detect`` (alias ``yolo-detect``) — box rectangles only, bootstrap path
    - ``yolov10`` (aliases ``yolov10n``, ``yolov10-nano``) — YOLOv10 NMS-free ONNX
      from ``onnx-community/yolov10n`` on Hugging Face. Detection only, uses ONNX
      Runtime directly (no Ultralytics dependency).
    - ``nanodet`` (aliases ``nanodet-plus``, ``nanodet-plus-m``) — NanoDet-Plus
      ultra-lightweight ONNX exported from the RangiLyu/nanodet repo. Detection
      only, uses ONNX Runtime directly.
    - ``cs2-sam`` (aliases ``cs2_sam``, ``cs2sam``, ``vombit-sam``, ``cs2-edge-sam``) —
      Two-stage hybrid: Vombit YOLOv10n CS2 player detection + EdgeSAM mask
      refinement. Produces team-aware player-shaped masks from a single pipeline.
      Requires downloading the EdgeSAM encoder/decoder ONNX models from
      ``chongzhou/EdgeSAM`` on Hugging Face.
    """
    normalized = normalize_segmenter_backend(backend)
    if normalized not in SUPPORTED_SEGMENTER_BACKENDS:
        supported = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
        raise ValueError(
            f"unknown or unsupported segmenter backend {backend!r}; supported: {supported}"
        )
    constructor = _constructor_for(normalized)
    segmenter = constructor(
        model_path,
        manifest_path,
        class_names=class_names,
        confidence=confidence,
        image_size=image_size,
        device=device,
    )
    segmenter.backend = normalized
    return segmenter
