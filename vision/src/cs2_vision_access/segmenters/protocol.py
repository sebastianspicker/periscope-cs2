"""Segmentation backend protocol and shared constants.

No adapter implementations here — only the contract, error type, backend-name
normalisation, and class-id resolution helpers that every adapter needs.
"""

from __future__ import annotations

from typing import Protocol

import numpy as np

from cs2_vision_access.model_manifest import ModelManifest
from cs2_vision_access.predictions import InstanceMask

DEFAULT_SEGMENTER_BACKEND = "ultralytics-onnx"
RFDETR_BACKEND = "rfdetr"
ULTRALYTICS_DETECT_BACKEND = "ultralytics-detect"
YOLOV10_BACKEND = "yolov10"
NANODET_BACKEND = "nanodet"
CS2_SAM_BACKEND = "cs2-sam"

# Aliases strip/casefold to a canonical key.
_BACKEND_ALIASES: dict[str, str] = {
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


class Segmenter(Protocol):
    """Minimum contract needed by the no-backlog video loop."""

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]: ...


class SegmenterError(RuntimeError):
    """A backend failed to produce a trustworthy current-frame result."""


def _require_bgr_frame(frame_bgr: np.ndarray, frame_index: int) -> None:
    """Validate frame_index and BGR HWC shape shared by predict() entry points."""
    if frame_index < 0:
        raise ValueError("frame_index must be non-negative")
    if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
        raise ValueError("frame_bgr must have shape (height, width, 3)")


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


def normalize_segmenter_backend(backend: str) -> str:
    """Strip, casefold, and apply known aliases (e.g. ``rf-detr`` → ``rfdetr``)."""
    normalized = backend.strip().casefold()
    return _BACKEND_ALIASES.get(normalized, normalized)


def resolve_class_ids(
    manifest: ModelManifest, requested_names: tuple[str, ...] | None
) -> frozenset[int]:
    """Resolve requested class names to manifest class IDs.

    When ``requested_names`` is None and the manifest has exactly one class,
    that class is returned.  Multi-class manifests require explicit ``--class-name``.
    """
    if requested_names is None:
        if len(manifest.classes) != 1:
            available = ", ".join(manifest.classes.values())
            raise ValueError(
                "a multi-class model requires at least one --class-name; "
                f"available classes: {available}"
            )
        return frozenset(manifest.classes)

    requested = {name.strip().casefold() for name in requested_names if name.strip()}
    if not requested:
        raise ValueError("--class-name values must not be empty")
    matched = {
        class_id for class_id, name in manifest.classes.items() if name.casefold() in requested
    }
    missing = sorted(requested - {name.casefold() for name in manifest.classes.values()})
    if missing:
        raise ValueError(f"requested classes are absent from manifest: {missing}")
    return frozenset(matched)
