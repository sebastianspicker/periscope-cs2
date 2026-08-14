"""Parse study package inference settings."""

from __future__ import annotations

import math

from cs2_vision_access.segmenters import (
    DEFAULT_SEGMENTER_BACKEND,
    SUPPORTED_SEGMENTER_BACKENDS,
    normalize_segmenter_backend,
)
from cs2_vision_access.study._util import _optional_text, _required_text
from cs2_vision_access.study.errors import StudyError
from cs2_vision_access.study.models import DEFAULT_MAX_FRAMES, StudyInference

_INFERENCE_REQUIRED = frozenset({"model", "manifest"})
_INFERENCE_OPTIONAL = frozenset(
    {
        "backend",
        "class_names",
        "confidence",
        "image_size",
        "device",
        "max_frames",
        "max_seconds",
    }
)
_INFERENCE_ALLOWED = _INFERENCE_REQUIRED | _INFERENCE_OPTIONAL


def _parse_inference(raw: object) -> StudyInference:
    if not isinstance(raw, dict):
        raise StudyError("inference must be a JSON object")
    keys = frozenset(raw)
    missing = sorted(_INFERENCE_REQUIRED - keys)
    unknown = sorted(keys - _INFERENCE_ALLOWED)
    if missing or unknown:
        raise StudyError(
            f"inference keys do not match schema; missing={missing}, unknown={unknown}"
        )
    model = _required_text(raw["model"], "inference.model")
    manifest = _required_text(raw["manifest"], "inference.manifest")
    backend = _optional_text(raw.get("backend"), "inference.backend")
    if backend is None:
        backend = DEFAULT_SEGMENTER_BACKEND
    else:
        normalized = normalize_segmenter_backend(backend)
        if normalized not in SUPPORTED_SEGMENTER_BACKENDS:
            supported = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
            raise StudyError(
                f"inference.backend unknown or unsupported: {backend!r}; supported: {supported}"
            )
        backend = normalized

    class_names: tuple[str, ...] | None = None
    if "class_names" in raw and raw["class_names"] is not None:
        names_raw = raw["class_names"]
        if not isinstance(names_raw, list) or not names_raw:
            raise StudyError("inference.class_names must be a non-empty string array")
        if not all(isinstance(name, str) and name.strip() for name in names_raw):
            raise StudyError("inference.class_names entries must be non-empty strings")
        class_names = tuple(name.strip() for name in names_raw)

    confidence = raw.get("confidence", 0.45)
    if isinstance(confidence, bool) or not isinstance(confidence, (int, float)):
        raise StudyError("inference.confidence must be a number")
    confidence_f = float(confidence)
    if not math.isfinite(confidence_f) or not 0.0 <= confidence_f <= 1.0:
        raise StudyError("inference.confidence must be finite and in [0, 1]")

    image_size = raw.get("image_size", 640)
    if isinstance(image_size, bool) or not isinstance(image_size, int) or image_size <= 0:
        raise StudyError("inference.image_size must be a positive integer")

    device = _optional_text(raw.get("device"), "inference.device")
    if device is None:
        device = "cpu"

    max_frames = raw.get("max_frames", DEFAULT_MAX_FRAMES)
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames <= 0:
        raise StudyError("inference.max_frames must be a positive integer")

    max_seconds = raw.get("max_seconds")
    if max_seconds is not None:
        if isinstance(max_seconds, bool) or not isinstance(max_seconds, (int, float)):
            raise StudyError("inference.max_seconds must be a number or null")
        max_seconds_f = float(max_seconds)
        if not math.isfinite(max_seconds_f) or max_seconds_f <= 0:
            raise StudyError("inference.max_seconds must be a finite positive number")
        max_seconds = max_seconds_f

    return StudyInference(
        model=model,
        manifest=manifest,
        backend=backend,
        class_names=class_names,
        confidence=confidence_f,
        image_size=image_size,
        device=device,
        max_frames=max_frames,
        max_seconds=max_seconds,
    )
