"""Pipeline lifecycle controller for the desktop GUI dashboard.

Pure threading + pipeline orchestration with no tkinter dependency, so the
controller is fully testable without a display. ``GuiApp`` (or any other
frontend) drives it.
"""

from __future__ import annotations

import argparse
import threading
from collections.abc import Callable
from pathlib import Path

import numpy as np

from cs2_vision_access.capture import (
    CaptureConfig,
    FileOutputSink,
    LiveControlState,
    OverlayWindow,
    ScreenCapturer,
    ScreenRegion,
    create_overlay,
    is_overlay_available,
    list_monitors,
)
from cs2_vision_access.capture.hotkeys import apply_hotkey_action
from cs2_vision_access.cli.style_args import _outline_style
from cs2_vision_access.gui.model import GuiSettings, build_segmenter
from cs2_vision_access.inference.pipeline import (
    LivePipelineConfig,
    LiveRunSummary,
    run_live_pipeline,
)
from cs2_vision_access.inference.temporal import TemporalStabilityConfig
from cs2_vision_access.renderer import OutlineStyle

_THREAD_JOIN_TIMEOUT_SECONDS = 3.0
_THREAD_NAME = "gui-live-pipeline"


def _noop_printer(_message: str) -> None:
    """Default status sink; real frontends replace it with a log writer."""


def build_live_pipeline_config(
    settings: GuiSettings,
    control_state: LiveControlState,
    terminate_event: threading.Event,
    overlay: OverlayWindow | None,
) -> LivePipelineConfig:
    """Assemble a :class:`LivePipelineConfig` from GUI settings.

    Pure function so it can be unit-tested without any display or capture
    hardware (capture objects are constructed here; callers may patch them).
    """
    capture: CaptureConfig | ScreenCapturer
    if settings.source_type == "screen":
        if settings.region is not None:
            if len(settings.region) != 4:
                raise ValueError("screen region must be (left, top, width, height)")
            left, top, region_width, region_height = settings.region
            capture = ScreenCapturer(region=ScreenRegion(left, top, region_width, region_height))
        else:
            capture = ScreenCapturer(monitor_index=settings.monitor_index)
    else:
        source: int | str = (
            settings.device_index
            if settings.source_type == "capture-device"
            else settings.file_path
        )
        capture = CaptureConfig(
            source=source,
            preferred_width=settings.width,
            preferred_height=settings.height,
            preferred_fps=settings.fps,
            backend=settings.backend,
        )

    return LivePipelineConfig(
        capture=capture,
        max_frames=settings.max_frames,
        display_scale=settings.display_scale,
        headless=True,
        alpha_output_mode=settings.output_mode,
        enable_alpha_fill=settings.alpha_fill,
        temporal_config=TemporalStabilityConfig(
            enabled=settings.temporal_enabled,
            min_consecutive_frames=settings.temporal_min_frames,
            hold_last_mask=settings.temporal_hold,
            max_dropout_frames=settings.temporal_max_dropout,
        ),
        output_sink=(
            FileOutputSink(Path(settings.output_sink), fps=settings.fps)
            if settings.output_sink
            else None
        ),
        overlay_window=overlay,
        control_state=control_state,
        terminate_event=terminate_event,
    )


def _resolve_overlay_position(settings: GuiSettings) -> tuple[int, int]:
    """Auto-align the overlay to a monitor's origin when x/y are left at 0."""
    x = settings.overlay_x
    y = settings.overlay_y
    for monitor in list_monitors():
        if int(monitor.get("index", -1)) == settings.overlay_monitor:
            if x == 0:
                x = int(monitor.get("left", 0))
            if y == 0:
                y = int(monitor.get("top", 0))
            break
    return x, y


class GuiPipelineController:
    """Owns the live pipeline thread, control state, overlay, and summary."""

    def __init__(self, settings: GuiSettings) -> None:
        self.settings = settings
        self.control_state = LiveControlState(
            output_mode=settings.output_mode,
            alpha_fill=settings.alpha_fill,
            temporal_enabled=settings.temporal_enabled,
        )
        self.terminate_event = threading.Event()
        self.overlay: OverlayWindow | None = None
        self._thread: threading.Thread | None = None
        self._last_summary: LiveRunSummary | None = None
        self._on_log: Callable[[str], None] = _noop_printer

    @property
    def is_running(self) -> bool:
        """Whether the pipeline worker thread is alive."""
        return self._thread is not None and self._thread.is_alive()

    @property
    def last_summary(self) -> LiveRunSummary | None:
        """Summary produced by the most recent completed run, if any."""
        return self._last_summary

    def start(
        self,
        on_frame: Callable[[np.ndarray, int, int], None],
        on_log: Callable[[str], None],
    ) -> None:
        """Build the pipeline pieces and launch the blocking run in a thread.

        The overlay is created on the calling (GUI main) thread. Startup
        failures (segmenter construction, style validation) are logged and the
        controller stays stopped instead of crashing the caller.
        """
        if self.is_running:
            return
        self._on_log = on_log
        on_log("Starting live pipeline")

        overlay = self._create_overlay(on_log)
        try:
            segmenter = build_segmenter(self.settings)
            style = self._resolve_style()
            config = build_live_pipeline_config(
                self.settings,
                self.control_state,
                self.terminate_event,
                overlay,
            )
        except Exception as error:
            on_log(f"Startup failed: {error}")
            if overlay is not None:
                overlay.close()
                self.overlay = None
            return
        self.terminate_event.clear()
        self._last_summary = None

        def _run() -> None:
            try:
                summary = run_live_pipeline(
                    segmenter=segmenter,
                    config=config,
                    outline_style=style,
                    on_frame=on_frame,
                )
                self._last_summary = summary
            except Exception as error:  # pragma: no cover - defensive
                self._last_summary = None
                on_log(f"Pipeline error: {error}")

        self._thread = threading.Thread(
            target=_run,
            name=_THREAD_NAME,
            daemon=True,
        )
        self._thread.start()
        on_log("Live pipeline running")

    def stop(self) -> None:
        """Request termination, join the worker, and release the overlay."""
        self.terminate_event.set()
        thread = self._thread
        if thread is not None and thread.is_alive():
            thread.join(timeout=_THREAD_JOIN_TIMEOUT_SECONDS)
        if self.overlay is not None:
            try:
                self.overlay.close()
            finally:
                self.overlay = None
        self._thread = None

    def apply_action(self, action: str) -> bool:
        """Apply a named live control action; True means quit was requested."""
        return apply_hotkey_action(action, self.control_state, printer=self._on_log)

    def _create_overlay(self, on_log: Callable[[str], None]) -> OverlayWindow | None:
        if not self.settings.overlay:
            return None
        if not is_overlay_available():
            on_log("Overlay requested but no backend is available; skipping")
            return None
        x = self.settings.overlay_x
        y = self.settings.overlay_y
        try:
            if self.settings.source_type == "screen" and self.settings.overlay_monitor > 0:
                x, y = _resolve_overlay_position(self.settings)
            overlay = create_overlay(
                self.settings.window_title,
                self.settings.width,
                self.settings.height,
                x=x,
                y=y,
            )
        except Exception as error:
            on_log(f"Could not create overlay window: {error}")
            return None
        self.overlay = overlay
        on_log("Overlay window created")
        return overlay

    def _resolve_style(self) -> OutlineStyle:
        """Resolve the outline style via the CLI's preset/flag machinery."""
        namespace = argparse.Namespace(
            prefs=None,
            outline_preset=self.settings.outline_preset,
            inner_color=self.settings.inner_color,
            outer_color=self.settings.outer_color,
            inner_width=self.settings.inner_width,
            outer_width=self.settings.outer_width,
            fill_opacity=self.settings.fill_opacity,
            fixed_widths=self.settings.fixed_widths,
            stroke_pattern=self.settings.stroke_pattern,
            dash_period=self.settings.dash_period,
            fill_mode=self.settings.fill_mode,
            halo_blur=self.settings.halo_blur,
            adapt_width=self.settings.adapt_width,
            outline_kernel=None,
        )
        return _outline_style(namespace)

    @staticmethod
    def format_summary(summary: LiveRunSummary) -> str:
        """Render a human-readable multi-line summary from a live run."""
        return "\n".join(
            (
                f"Frames processed:    {summary.frames_processed}",
                f"Frames inferred:     {summary.frames_inferred}",
                f"Frames dropped:      {summary.frames_dropped}",
                f"Instances predicted: {summary.instances_predicted}",
                f"Instances outlined:  {summary.instances_outlined}",
                f"Temporal suppressed: {summary.temporal_suppressed}",
                "Inference p50/p95:   "
                f"{summary.inference_ms_p50:.1f} / {summary.inference_ms_p95:.1f} ms",
                "Pipeline p50/p95:    "
                f"{summary.pipeline_ms_p50:.1f} / {summary.pipeline_ms_p95:.1f} ms",
                f"Elapsed:             {summary.elapsed_seconds:.1f} s",
                f"Capture FPS:         {summary.capture_fps:.1f}",
                f"Throughput FPS:      {summary.throughput_fps:.1f}",
                f"Termination:         {summary.termination_reason}",
            )
        )
