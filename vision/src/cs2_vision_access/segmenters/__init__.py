"""Segmentation backends — protocol, factory, and implementations.

Cleanly separates the ``Segmenter`` protocol and backend normalization
(``segmenters.protocol``) from the factory (``segmenters.factory``) and the
individual adapter implementations (``segmenters.ultralytics``,
``segmenters.detect``, ``segmenters.rfdetr``).

Eliminates the lazy import back-edge that existed when protocol and factory
lived in the same module as one of the adapters.
"""

from __future__ import annotations

from cs2_vision_access.segmenters.factory import (
    create_segmenter,
)
from cs2_vision_access.segmenters.protocol import (
    CS2_SAM_BACKEND,
    DEFAULT_SEGMENTER_BACKEND,
    NANODET_BACKEND,
    RFDETR_BACKEND,
    SUPPORTED_SEGMENTER_BACKENDS,
    ULTRALYTICS_DETECT_BACKEND,
    YOLOV10_BACKEND,
    Segmenter,
    SegmenterError,
    normalize_segmenter_backend,
)
from cs2_vision_access.segmenters.ultralytics import UltralyticsOnnxSegmenter

__all__ = [
    "CS2_SAM_BACKEND",
    "DEFAULT_SEGMENTER_BACKEND",
    "NANODET_BACKEND",
    "RFDETR_BACKEND",
    "SUPPORTED_SEGMENTER_BACKENDS",
    "Segmenter",
    "SegmenterError",
    "ULTRALYTICS_DETECT_BACKEND",
    "UltralyticsOnnxSegmenter",
    "YOLOV10_BACKEND",
    "create_segmenter",
    "normalize_segmenter_backend",
]
