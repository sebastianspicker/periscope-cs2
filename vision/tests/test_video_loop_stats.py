"""Video loop contract tests: summaries, temporal/cues, and preview mode."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from dataclasses import asdict, fields
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from cs2_vision_access.renderer import OutlineStyle, RenderDiagnostics
from cs2_vision_access.video import (
    VideoProcessingError,
    VideoRunSummary,
    _outline_style_hash,
    process_video,
)
from tests.video_loop_helpers import (
    _Capture,
    _fake_cv2,
    _Renderer,
    _Segmenter,
)


class VideoLoopStatsAndCuesTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.input_path = Path(self.temporary_directory.name) / "fixture.mp4"
        self.input_path.write_bytes(b"video fixture")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_summary_records_backend_and_style_identifiers(self) -> None:
        events: list[str] = []
        frames = [np.zeros((12, 16, 3), dtype=np.uint8)]
        capture = _Capture(frames, events)
        style = OutlineStyle()
        expected_hash = hashlib.sha256(
            json.dumps(
                asdict(style),
                sort_keys=True,
                separators=(",", ":"),
                default=str,
            ).encode("utf-8")
        ).hexdigest()

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events, backend="ultralytics-onnx"),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=False,
            )

        self.assertEqual(summary.segmenter_backend, "ultralytics-onnx")
        self.assertEqual(summary.outline_style_hash, expected_hash)
        self.assertEqual(summary.outline_stroke_pattern, "solid")
        self.assertEqual(_outline_style_hash(style), expected_hash)

    def test_summary_backend_absent_when_segmenter_has_no_name(self) -> None:
        events: list[str] = []
        capture = _Capture([np.zeros((12, 16, 3), dtype=np.uint8)], events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=False,
            )

        self.assertIsNone(summary.segmenter_backend)
        self.assertIsNotNone(summary.outline_style_hash)
        self.assertEqual(summary.outline_stroke_pattern, "solid")

    def test_summary_as_dict_keeps_existing_keys_and_adds_optional_fields(
        self,
    ) -> None:
        """Historical summary keys must remain; new fields are additive only."""
        legacy_keys = (
            "frames_processed",
            "instances_predicted",
            "instances_outlined",
            "stale_predictions_discarded",
            "degenerate_masks_discarded",
            "source_fps",
            "source_width",
            "source_height",
            "frame_budget_ms",
            "frame_budget_misses",
            "elapsed_seconds",
            "throughput_fps",
            "inference_ms_p50",
            "inference_ms_p95",
            "pipeline_ms_p50",
            "pipeline_ms_p95",
            "completed_stream",
            "termination_reason",
            "outline_inner_color",
            "outline_outer_color",
            "outline_fill_opacity",
            "outline_inner_width_pixels",
            "outline_outer_width_pixels",
            "outline_stroke_contrast_ratio",
            "output_path",
        )
        additive_keys = (
            "segmenter_backend",
            "outline_style_hash",
            "outline_stroke_pattern",
            "cue_log_path",
            "cue_events_written",
            "temporal_suppressed",
        )
        field_names = tuple(field.name for field in fields(VideoRunSummary))
        self.assertEqual(field_names[: len(legacy_keys)], legacy_keys)
        self.assertEqual(field_names[len(legacy_keys) :], additive_keys)

        events: list[str] = []
        capture = _Capture([np.zeros((12, 16, 3), dtype=np.uint8)], events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events, backend="ultralytics-onnx"),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=False,
            )

        payload = summary.as_dict()
        for key in legacy_keys:
            self.assertIn(key, payload)
        for key in additive_keys:
            self.assertIn(key, payload)
        self.assertEqual(payload["segmenter_backend"], "ultralytics-onnx")
        self.assertEqual(payload["outline_stroke_pattern"], "solid")
        self.assertIsInstance(payload["outline_style_hash"], str)
        self.assertEqual(len(payload["outline_style_hash"]), 64)
        self.assertIsNone(payload["cue_log_path"])
        self.assertIsNone(payload["cue_events_written"])
        self.assertEqual(payload["temporal_suppressed"], 0)

    def test_temporal_suppress_filters_before_render_and_counts(self) -> None:
        from cs2_vision_access.inference.temporal import TemporalStabilityConfig
        from cs2_vision_access.predictions import InstanceMask

        events: list[str] = []
        frames = [np.zeros((12, 16, 3), dtype=np.uint8) for _ in range(3)]
        capture = _Capture(frames, events)
        polygon = ((1.0, 1.0), (5.0, 1.0), (5.0, 5.0), (1.0, 5.0))

        class _MaskSegmenter:
            backend = "ultralytics-onnx"

            def predict(self, _frame: np.ndarray, *, frame_index: int):
                events.append(f"infer:{frame_index}")
                return (
                    InstanceMask(
                        frame_index=frame_index,
                        polygon=polygon,
                        confidence=0.9,
                        class_id=0,
                        class_name="player",
                    ),
                )

        class _CountingRenderer(_Renderer):
            def render_with_diagnostics(
                self,
                frame: np.ndarray,
                predictions: tuple[object, ...],
                *,
                frame_index: int,
            ) -> tuple[np.ndarray, RenderDiagnostics]:
                events.append(f"render:{frame_index}:{len(predictions)}")
                return (
                    frame.copy(),
                    RenderDiagnostics(
                        predictions_received=len(predictions),
                        current_predictions=len(predictions),
                        stale_predictions_discarded=0,
                        degenerate_masks_discarded=0,
                        contours_rendered=len(predictions),
                        inner_width_pixels=3,
                        outer_width_pixels=7,
                        stroke_contrast_ratio=10.0,
                        stroke_pattern="solid",
                        dash_period_pixels=12,
                    ),
                )

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_MaskSegmenter(),
                renderer=_CountingRenderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=3,
                overwrite=False,
                temporal_config=TemporalStabilityConfig(
                    enabled=True,
                    min_consecutive_frames=2,
                ),
            )

        self.assertEqual(summary.instances_predicted, 3)
        self.assertEqual(summary.temporal_suppressed, 1)
        self.assertEqual(summary.instances_outlined, 2)
        self.assertIn("render:0:0", events)
        self.assertIn("render:1:1", events)
        self.assertIn("render:2:1", events)

    def test_cue_log_records_enter_leave_and_summary_fields(self) -> None:
        from cs2_vision_access.cues import load_cue_events
        from cs2_vision_access.predictions import InstanceMask

        events: list[str] = []
        frames = [
            np.zeros((12, 16, 3), dtype=np.uint8),
            np.zeros((12, 16, 3), dtype=np.uint8),
            np.zeros((12, 16, 3), dtype=np.uint8),
        ]
        capture = _Capture(frames, events)
        polygon = ((1.0, 1.0), (5.0, 1.0), (5.0, 5.0), (1.0, 5.0))

        class _MaskSegmenter:
            backend = "ultralytics-onnx"

            def predict(self, _frame: np.ndarray, *, frame_index: int):
                events.append(f"infer:{frame_index}")
                if frame_index < 2:
                    return (
                        InstanceMask(
                            frame_index=frame_index,
                            polygon=polygon,
                            confidence=0.9,
                            class_id=0,
                            class_name="player",
                        ),
                    )
                return ()

        cue_path = Path(self.temporary_directory.name) / "run-cues.jsonl"
        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_MaskSegmenter(),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=3,
                overwrite=False,
                cue_log_path=cue_path,
            )

        self.assertEqual(summary.cue_log_path, str(cue_path.resolve()))
        self.assertIsNotNone(summary.cue_events_written)
        self.assertGreaterEqual(summary.cue_events_written or 0, 2)
        cue_events = load_cue_events(cue_path)
        self.assertEqual(cue_events[0].event, "enter")
        self.assertEqual(cue_events[0].frame_index, 0)
        leave_events = [event for event in cue_events if event.event == "leave"]
        self.assertEqual(len(leave_events), 1)
        self.assertEqual(leave_events[0].frame_index, 2)
        partial = cue_path.with_name(f".{cue_path.stem}.partial{cue_path.suffix}")
        self.assertFalse(partial.exists())

    def test_failed_cue_log_overwrite_preserves_prior_destination(self) -> None:

        prior = "prior cue log body\n"
        cue_path = Path(self.temporary_directory.name) / "run-cues.jsonl"
        cue_path.write_text(prior, encoding="utf-8")
        events: list[str] = []
        capture = _Capture([np.zeros((12, 16, 3), dtype=np.uint8)], events)

        class _FailingSegmenter:
            backend = "ultralytics-onnx"

            def predict(self, _frame: np.ndarray, *, frame_index: int):
                events.append(f"infer:{frame_index}")
                raise RuntimeError("inference fixture failed")

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaises(RuntimeError):
                process_video(
                    input_path=self.input_path,
                    segmenter=_FailingSegmenter(),
                    renderer=_Renderer(events),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=1,
                    overwrite=True,
                    cue_log_path=cue_path,
                )

        self.assertEqual(cue_path.read_text(encoding="utf-8"), prior)
        partial = cue_path.with_name(f".{cue_path.stem}.partial{cue_path.suffix}")
        self.assertFalse(partial.exists())

    def test_preview_frame_infers_only_target_and_writes_png(self) -> None:
        events: list[str] = []
        frames = [np.zeros((12, 16, 3), dtype=np.uint8) for _ in range(4)]
        capture = _Capture(frames, events)
        written: list[str] = []

        def imwrite(path: str, frame: np.ndarray) -> bool:
            written.append(path)
            Path(path).write_bytes(b"png-fixture")
            self.assertEqual(frame.shape, (12, 16, 3))
            return True

        fake_cv2 = SimpleNamespace(
            CAP_PROP_FPS=1,
            CAP_PROP_FRAME_WIDTH=2,
            CAP_PROP_FRAME_HEIGHT=3,
            VideoCapture=lambda _path: capture,
            imwrite=imwrite,
        )
        output_path = Path(self.temporary_directory.name) / "preview.png"

        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events),
                renderer=_Renderer(events),
                output_path=output_path,
                display=False,
                realtime_playback=False,
                max_frames=100,
                overwrite=False,
                preview_frame=2,
            )

        self.assertEqual(
            events,
            [
                "decode:0",
                "decode:1",
                "decode:2",
                "infer:2",
                "render:2",
            ],
        )
        self.assertEqual(summary.frames_processed, 3)
        self.assertEqual(summary.instances_predicted, 1)
        self.assertEqual(summary.instances_outlined, 1)
        self.assertEqual(summary.termination_reason, "preview_frame")
        self.assertFalse(summary.completed_stream)
        self.assertEqual(summary.output_path, str(output_path.resolve()))
        self.assertTrue(output_path.exists())
        self.assertEqual(len(written), 1)
        self.assertTrue(written[0].endswith(".partial.png"))
        partial = output_path.with_name(f".{output_path.stem}.partial{output_path.suffix}")
        self.assertFalse(partial.exists())
        self.assertTrue(capture.released)

    def test_preview_frame_beyond_video_fails_closed(self) -> None:
        events: list[str] = []
        capture = _Capture(
            [np.zeros((12, 16, 3), dtype=np.uint8) for _ in range(2)],
            events,
        )

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaisesRegex(VideoProcessingError, "beyond the video"):
                process_video(
                    input_path=self.input_path,
                    segmenter=_Segmenter(events),
                    renderer=_Renderer(events),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=10,
                    overwrite=False,
                    preview_frame=5,
                )

        self.assertEqual(events, ["decode:0", "decode:1"])
        self.assertNotIn("infer:0", events)
        self.assertTrue(capture.released)

    def test_preview_frame_rejects_negative_index(self) -> None:
        with self.assertRaisesRegex(ValueError, "preview_frame"):
            process_video(
                input_path=self.input_path,
                segmenter=_Segmenter([]),
                renderer=_Renderer([]),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=False,
                preview_frame=-1,
            )


if __name__ == "__main__":
    unittest.main()
