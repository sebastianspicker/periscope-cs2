"""Style / segmenter metadata for video run summaries."""

from __future__ import annotations

import hashlib
import json
from dataclasses import asdict

from cs2_vision_access.application.ports.segmentation import Segmenter
from cs2_vision_access.domain.outline import OutlineStyle

# Fallback when a style object lacks stroke_pattern (tests / older fixtures).
_DEFAULT_STROKE_PATTERN = "solid"


def _segmenter_backend_name(segmenter: Segmenter) -> str | None:
    """Return a duck-typed backend name when the segmenter exposes one."""
    value = getattr(segmenter, "backend", None)
    if value is None:
        return None
    text = str(value).strip() if not isinstance(value, str) else value.strip()
    return text or None


def _outline_style_hash(style: OutlineStyle) -> str:
    """Stable SHA-256 hex digest of the serialised OutlineStyle fields."""
    payload = asdict(style)
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"), default=str)
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def _outline_stroke_pattern(style: OutlineStyle) -> str:
    """Return stroke pattern from style, or the solid default when missing."""
    value = getattr(style, "stroke_pattern", None)
    if isinstance(value, str) and value.strip():
        return value.strip()
    return _DEFAULT_STROKE_PATTERN
