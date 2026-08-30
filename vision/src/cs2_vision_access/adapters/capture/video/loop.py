"""Frame-loop helpers for sequential video processing."""

from __future__ import annotations

import math
import time
from typing import Any

from cs2_vision_access.adapters.capture.video.stats import _percentile
from cs2_vision_access.adapters.capture.video.style_meta import (
    _outline_stroke_pattern,
    _outline_style_hash,
    _segmenter_backend_name,
)
from cs2_vision_access.application.live.inference.temporal import SuppressOnlyTemporalPolicy
from cs2_vision_access.application.ports.rendering import CueLogWriter, OutlineRenderer
from cs2_vision_access.application.ports.segmentation import Segmenter
from cs2_vision_access.domain.outline import contrast_ratio, effective_line_widths
from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.domain.video import VideoRunSummary


def _current_frame_masks(
    predictions: tuple[InstanceMask, ...], frame_index: int
) -> tuple[InstanceMask, ...]:
    return tuple(prediction for prediction in predictions if prediction.frame_index == frame_index)


def resolve_effective_frame_limit(
    *,
    max_frames: int,
    max_seconds: float | None,
    preview_frame: int | None,
    fps: float,
) -> tuple[int, str]:
    """Return ``(effective_frame_limit, limit_reason)`` for the run."""
    if preview_frame is not None:
        # Single-frame path: decode up to the target, infer once, stop.
        return preview_frame + 1, "preview_frame"
    effective_frame_limit = max_frames
    limit_reason = "frame_limit"
    if max_seconds is not None:
        duration_frame_limit = max(1, math.ceil(max_seconds * fps))
        if duration_frame_limit <= effective_frame_limit:
            effective_frame_limit = duration_frame_limit
            limit_reason = "duration_limit"
    return effective_frame_limit, limit_reason


def infer_render_and_observe(
    *,
    frame: Any,
    frame_index: int,
    segmenter: Segmenter,
    renderer: OutlineRenderer,
    cue_writer: CueLogWriter | None,
    inference_ms: list[float],
    temporal_policy: SuppressOnlyTemporalPolicy | None = None,
) -> tuple[Any, int, int, int, int, int]:
    """Infer, optionally stabilize, render, and write cue events for one frame.

    Temporal suppress-only filtering (when enabled) runs **between** predict and
    render: unstable detections are dropped, never held. Returns
    ``(rendered, prediction_count, outlined, stale, degenerate, temporal_suppressed)``.
    """
    inference_started = time.perf_counter()
    predictions = segmenter.predict(frame, frame_index=frame_index)
    inference_ms.append((time.perf_counter() - inference_started) * 1000.0)
    prediction_count = len(predictions)
    temporal_suppressed = 0
    if temporal_policy is not None:
        predictions, temporal_diagnostics = temporal_policy.filter(
            predictions,
            frame_index=frame_index,
        )
        temporal_suppressed = temporal_diagnostics.suppressed_count
    rendered, render_diagnostics = renderer.render_with_diagnostics(
        frame,
        predictions,
        frame_index=frame_index,
    )
    if cue_writer is not None:
        # Cue log follows the same frame binding as the renderer: stale and
        # temporally suppressed predictions never produce enter/leave events.
        cue_writer.observe(
            frame_index,
            _current_frame_masks(predictions, frame_index),
        )
    return (
        rendered,
        prediction_count,
        render_diagnostics.contours_rendered,
        render_diagnostics.stale_predictions_discarded,
        render_diagnostics.degenerate_masks_discarded,
        temporal_suppressed,
    )


def poll_display_stop(
    cv2: Any,
    rendered: Any,
    *,
    image_preview: bool,
    display: bool,
) -> bool:
    """Show the frame when requested; return True if the user asked to stop."""
    if not display:
        return False
    cv2.imshow("CS2 Vision Access - recorded footage only", rendered)
    # Single-frame preview waits for a key so the image is inspectable;
    # full runs use a short poll so q/Esc can stop without blocking.
    wait_ms = 0 if image_preview else 1
    key = cv2.waitKey(wait_ms) & 0xFF
    return key in (27, ord("q"))


def pace_realtime(
    *,
    started: float,
    frame_index: int,
    fps: float,
    realtime_playback: bool,
) -> None:
    """Sleep to match source FPS when realtime playback is enabled."""
    if not realtime_playback:
        return
    deadline = started + (frame_index / fps)
    remaining = deadline - time.perf_counter()
    if remaining > 0:
        time.sleep(remaining)


def build_run_summary(
    *,
    frames_processed: int,
    prediction_count: int,
    outlined_count: int,
    stale_count: int,
    degenerate_count: int,
    fps: float,
    width: int,
    height: int,
    frame_budget_misses: int,
    started: float,
    inference_ms: list[float],
    pipeline_ms: list[float],
    completed_stream: bool,
    termination_reason: str,
    renderer: OutlineRenderer,
    segmenter: Segmenter,
    destination: Any,
    resolved_cue_log_path: str | None,
    cue_events_written: int | None,
    temporal_suppressed: int = 0,
) -> VideoRunSummary:
    """Assemble the immutable run summary from loop counters and style meta."""
    elapsed = time.perf_counter() - started
    # Preview counts every decoded frame up to the target so the summary
    # remains comparable to short-segment runs; inference counts stay at one.
    inner_width, outer_width = effective_line_widths(renderer.style, height)
    return VideoRunSummary(
        frames_processed=frames_processed,
        instances_predicted=prediction_count,
        instances_outlined=outlined_count,
        stale_predictions_discarded=stale_count,
        degenerate_masks_discarded=degenerate_count,
        source_fps=fps,
        source_width=width,
        source_height=height,
        frame_budget_ms=1000.0 / fps,
        frame_budget_misses=frame_budget_misses,
        elapsed_seconds=elapsed,
        throughput_fps=(frames_processed / elapsed) if elapsed > 0 else 0.0,
        inference_ms_p50=_percentile(inference_ms, 50),
        inference_ms_p95=_percentile(inference_ms, 95),
        pipeline_ms_p50=_percentile(pipeline_ms, 50),
        pipeline_ms_p95=_percentile(pipeline_ms, 95),
        completed_stream=completed_stream,
        termination_reason=termination_reason,
        outline_inner_color=renderer.style.inner_color,
        outline_outer_color=renderer.style.outer_color,
        outline_fill_opacity=renderer.style.fill_opacity,
        outline_inner_width_pixels=inner_width,
        outline_outer_width_pixels=outer_width,
        outline_stroke_contrast_ratio=contrast_ratio(
            renderer.style.inner_color,
            renderer.style.outer_color,
        ),
        output_path=str(destination) if destination is not None else None,
        segmenter_backend=_segmenter_backend_name(segmenter),
        outline_style_hash=_outline_style_hash(renderer.style),
        outline_stroke_pattern=_outline_stroke_pattern(renderer.style),
        cue_log_path=resolved_cue_log_path,
        cue_events_written=cue_events_written,
        temporal_suppressed=temporal_suppressed,
    )
