"""Sequential, file-only video processing with no prediction backlog.

Re-exports the public API previously provided by the monofile ``video`` module.
"""

from __future__ import annotations

from cs2_vision_access.adapters.capture.video.process import process_video
from cs2_vision_access.adapters.capture.video.stats import _percentile
from cs2_vision_access.adapters.capture.video.style_meta import (
    _DEFAULT_STROKE_PATTERN,
    _outline_stroke_pattern,
    _outline_style_hash,
    _segmenter_backend_name,
)
from cs2_vision_access.domain.video import (
    VideoProcessingError,
    VideoRunSummary,
)

__all__ = [
    "VideoProcessingError",
    "VideoRunSummary",
    "process_video",
    # Private names retained for tests that import helpers from the package.
    "_DEFAULT_STROKE_PATTERN",
    "_outline_stroke_pattern",
    "_outline_style_hash",
    "_percentile",
    "_segmenter_backend_name",
]
