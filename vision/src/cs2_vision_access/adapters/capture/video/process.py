"""Sequential, file-only video processing orchestration."""

from __future__ import annotations

import math
import os
import time
from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.adapters.capture.video.loop import (
    build_run_summary,
    infer_render_and_observe,
    pace_realtime,
    poll_display_stop,
    resolve_effective_frame_limit,
)
from cs2_vision_access.application.live.inference.temporal import (
    SuppressOnlyTemporalPolicy,
    TemporalStabilityConfig,
)
from cs2_vision_access.application.ports.rendering import CueLogWriter, OutlineRenderer
from cs2_vision_access.application.ports.segmentation import Segmenter
from cs2_vision_access.domain.safety import (
    MAX_FRAME_PIXELS,
    InputSafetyError,
    validate_decoded_frame,
    validate_image_output,
    validate_video_input,
    validate_video_output,
)
from cs2_vision_access.domain.video import (
    VideoProcessingError,
    VideoRunSummary,
)


def process_video(
    *,
    input_path: str | Path,
    segmenter: Segmenter,
    renderer: OutlineRenderer,
    output_path: str | Path | None,
    display: bool,
    realtime_playback: bool,
    max_frames: int,
    overwrite: bool,
    max_seconds: float | None = None,
    cue_log_path: str | Path | None = None,
    preview_frame: int | None = None,
    temporal_config: TemporalStabilityConfig | None = None,
    cue_writer_factory: Callable[[str | Path, bool], CueLogWriter] | None = None,
) -> VideoRunSummary:
    """Process one local file synchronously; a prediction never outlives its frame.

    When ``preview_frame`` is set, only that zero-based frame is inferred and
    rendered. Output may be a single PNG (A/B style check without a full encode)
    and/or a windowed preview. Frames before the target are decoded only.
    """
    if max_frames <= 0:
        raise ValueError("max_frames must be positive")
    if max_seconds is not None and (not math.isfinite(max_seconds) or max_seconds <= 0):
        raise ValueError("max_seconds must be a finite positive number")
    if preview_frame is not None and (
        isinstance(preview_frame, bool) or not isinstance(preview_frame, int) or preview_frame < 0
    ):
        raise ValueError("preview_frame must be a non-negative integer")
    source = validate_video_input(input_path)
    image_preview = preview_frame is not None
    if image_preview:
        destination = (
            validate_image_output(output_path, overwrite=overwrite)
            if output_path is not None
            else None
        )
    else:
        destination = (
            validate_video_output(output_path, overwrite=overwrite)
            if output_path is not None
            else None
        )
    if destination is not None and destination == source:
        raise VideoProcessingError("video input and output must be different files")

    try:
        import cv2
    except ImportError as error:  # pragma: no cover - dependency boundary
        raise VideoProcessingError(
            "OpenCV is required for video decoding; install the project dependencies"
        ) from error

    capture = cv2.VideoCapture(str(source))
    writer = None
    partial_output: Path | None = None
    output_committed = False
    started = time.perf_counter()
    inference_ms: list[float] = []
    pipeline_ms: list[float] = []
    frame_index = 0
    prediction_count = 0
    outlined_count = 0
    stale_count = 0
    degenerate_count = 0
    frame_budget_misses = 0
    completed_stream = True
    termination_reason = "end_of_stream"
    processing_succeeded = False
    fps = 30.0
    width = 0
    height = 0
    cue_writer: CueLogWriter | None = None
    resolved_cue_log_path: str | None = None
    cue_events_written: int | None = None
    temporal_suppressed_count = 0
    stability = temporal_config if temporal_config is not None else TemporalStabilityConfig()
    temporal_policy: SuppressOnlyTemporalPolicy | None = (
        SuppressOnlyTemporalPolicy(stability) if stability.enabled else None
    )

    try:
        if cue_log_path is not None:
            try:
                if cue_writer_factory is None:
                    raise VideoProcessingError(
                        "cue logging requires a configured persistence adapter"
                    )
                cue_writer = cue_writer_factory(cue_log_path, overwrite)
            except ValueError as error:
                raise VideoProcessingError(str(error)) from error
            resolved_cue_log_path = str(cue_writer.path)

        if not capture.isOpened():
            raise VideoProcessingError(f"OpenCV could not open video: {source}")
        fps = float(capture.get(cv2.CAP_PROP_FPS))
        if not 0.1 <= fps <= 1000:
            fps = 30.0
        frame_budget_ms = 1000.0 / fps
        effective_frame_limit, limit_reason = resolve_effective_frame_limit(
            max_frames=max_frames,
            max_seconds=max_seconds,
            preview_frame=preview_frame,
            fps=fps,
        )
        if destination is not None:
            partial_output = destination.with_name(
                f".{destination.stem}.partial{destination.suffix}"
            )
            if partial_output.exists():
                raise VideoProcessingError(
                    f"stale partial output exists: {partial_output}; remove it explicitly"
                )

        while frame_index < effective_frame_limit:
            pipeline_started = time.perf_counter()
            ok, frame = capture.read()
            if not ok:
                if frame_index == 0:
                    raise VideoProcessingError("video contained no decodable frames")
                if image_preview:
                    raise VideoProcessingError(
                        f"preview frame {preview_frame} is beyond the video length "
                        f"(decoded {frame_index} frames)"
                    )
                break

            try:
                frame_width, frame_height = validate_decoded_frame(
                    frame,
                    expected_dimensions=((width, height) if frame_index > 0 else None),
                    max_frame_pixels=MAX_FRAME_PIXELS,
                )
            except InputSafetyError as error:
                raise VideoProcessingError(str(error)) from error
            if frame_index == 0:
                width, height = frame_width, frame_height
                if partial_output is not None and not image_preview:
                    writer = cv2.VideoWriter(
                        str(partial_output),
                        cv2.VideoWriter_fourcc(*"mp4v"),
                        fps,
                        (width, height),
                    )
                    if not writer.isOpened():
                        raise VideoProcessingError(
                            f"OpenCV could not create output video: {partial_output}"
                        )

            # Preview path: decode-only until the requested frame index.
            if image_preview and frame_index != preview_frame:
                frame_index += 1
                continue

            (
                rendered,
                frame_predictions,
                frame_outlined,
                frame_stale,
                frame_degenerate,
                frame_temporal_suppressed,
            ) = infer_render_and_observe(
                frame=frame,
                frame_index=frame_index,
                segmenter=segmenter,
                renderer=renderer,
                cue_writer=cue_writer,
                inference_ms=inference_ms,
                temporal_policy=temporal_policy,
            )
            prediction_count += frame_predictions
            outlined_count += frame_outlined
            stale_count += frame_stale
            degenerate_count += frame_degenerate
            temporal_suppressed_count += frame_temporal_suppressed

            if writer is not None:
                writer.write(rendered)
            if (
                image_preview
                and partial_output is not None
                and not cv2.imwrite(str(partial_output), rendered)
            ):
                raise VideoProcessingError(
                    f"OpenCV could not write preview image: {partial_output}"
                )
            stop_requested = poll_display_stop(
                cv2,
                rendered,
                image_preview=image_preview,
                display=display,
            )
            pipeline_elapsed_ms = (time.perf_counter() - pipeline_started) * 1000.0
            pipeline_ms.append(pipeline_elapsed_ms)
            if pipeline_elapsed_ms > frame_budget_ms:
                frame_budget_misses += 1
            frame_index += 1
            if stop_requested:
                completed_stream = False
                termination_reason = "user_stop"
                break
            if image_preview:
                completed_stream = False
                termination_reason = "preview_frame"
                break
            pace_realtime(
                started=started,
                frame_index=frame_index,
                fps=fps,
                realtime_playback=realtime_playback,
            )
        else:
            completed_stream = False
            termination_reason = limit_reason
        processing_succeeded = True
    finally:
        capture.release()
        if writer is not None:
            writer.release()
        if display:
            cv2.destroyAllWindows()
        if cue_writer is not None:
            # Atomic replace on success; discard partial and leave any prior
            # destination untouched on failure (mirrors video partial output).
            cue_writer.close(commit=processing_succeeded)
            if processing_succeeded:
                cue_events_written = cue_writer.events_written
                resolved_cue_log_path = str(cue_writer.path)
            else:
                resolved_cue_log_path = None
                cue_events_written = None
        if partial_output is not None and not processing_succeeded:
            partial_output.unlink(missing_ok=True)

    if partial_output is not None and destination is not None:
        try:
            os.replace(partial_output, destination)
            output_committed = True
        finally:
            if not output_committed:
                partial_output.unlink(missing_ok=True)

    return build_run_summary(
        frames_processed=frame_index,
        prediction_count=prediction_count,
        outlined_count=outlined_count,
        stale_count=stale_count,
        degenerate_count=degenerate_count,
        fps=fps,
        width=width,
        height=height,
        frame_budget_misses=frame_budget_misses,
        started=started,
        inference_ms=inference_ms,
        pipeline_ms=pipeline_ms,
        completed_stream=completed_stream,
        termination_reason=termination_reason,
        renderer=renderer,
        segmenter=segmenter,
        destination=destination,
        resolved_cue_log_path=resolved_cue_log_path,
        cue_events_written=cue_events_written,
        temporal_suppressed=temporal_suppressed_count,
    )
