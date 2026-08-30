"""Video run summary and processing error types."""

from __future__ import annotations

from dataclasses import asdict, dataclass


class VideoProcessingError(RuntimeError):
    """The local media pipeline could not safely complete."""


@dataclass(frozen=True)
class VideoRunSummary:
    frames_processed: int
    instances_predicted: int
    instances_outlined: int
    stale_predictions_discarded: int
    degenerate_masks_discarded: int
    source_fps: float
    source_width: int
    source_height: int
    frame_budget_ms: float
    frame_budget_misses: int
    elapsed_seconds: float
    throughput_fps: float
    inference_ms_p50: float
    inference_ms_p95: float
    pipeline_ms_p50: float
    pipeline_ms_p95: float
    completed_stream: bool
    termination_reason: str
    outline_inner_color: str
    outline_outer_color: str
    outline_fill_opacity: float
    outline_inner_width_pixels: int
    outline_outer_width_pixels: int
    outline_stroke_contrast_ratio: float
    output_path: str | None
    # Additive optional fields (defaults preserve callers that construct summaries
    # with the historical required set only). Existing keys remain stable.
    segmenter_backend: str | None = None
    outline_style_hash: str | None = None
    outline_stroke_pattern: str | None = None
    cue_log_path: str | None = None
    cue_events_written: int | None = None
    # Suppress-only temporal filter: count of predicted masks dropped for
    # not yet reaching min_consecutive_frames. Default 0 when policy is off.
    temporal_suppressed: int = 0

    def as_dict(self) -> dict[str, object]:
        return asdict(self)
