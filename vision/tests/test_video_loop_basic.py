"""Video loop contract tests: decode/infer/render sequencing and bounds."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from cs2_vision_access.video import VideoProcessingError, process_video
from tests.video_loop_helpers import (
    _Capture,
    _fake_cv2,
    _Renderer,
    _Segmenter,
    _Writer,
)


class VideoLoopBasicTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.input_path = Path(self.temporary_directory.name) / "fixture.mp4"
        self.input_path.write_bytes(b"video fixture")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_decode_infer_render_is_sequential_and_bounded(self) -> None:
        events: list[str] = []
        frames = [np.zeros((12, 16, 3), dtype=np.uint8) for _ in range(3)]
        capture = _Capture(frames, events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=2,
                overwrite=False,
            )

        self.assertEqual(
            events,
            [
                "decode:0",
                "infer:0",
                "render:0",
                "decode:1",
                "infer:1",
                "render:1",
            ],
        )
        self.assertEqual(summary.frames_processed, 2)
        self.assertEqual(summary.instances_predicted, 2)
        self.assertEqual(summary.instances_outlined, 2)
        self.assertEqual(summary.stale_predictions_discarded, 0)
        self.assertEqual(summary.degenerate_masks_discarded, 0)
        self.assertAlmostEqual(summary.frame_budget_ms, 1000.0 / 30.0)
        self.assertGreaterEqual(summary.pipeline_ms_p95, summary.pipeline_ms_p50)
        self.assertEqual((summary.source_width, summary.source_height), (16, 12))
        self.assertEqual(summary.outline_inner_color, "#F6FF00")
        self.assertGreaterEqual(summary.outline_stroke_contrast_ratio, 3.0)
        self.assertFalse(summary.completed_stream)
        self.assertEqual(summary.termination_reason, "frame_limit")
        self.assertTrue(capture.released)

    def test_source_duration_bound_stops_without_user_input(self) -> None:
        events: list[str] = []
        frames = [np.zeros((12, 16, 3), dtype=np.uint8) for _ in range(4)]
        capture = _Capture(frames, events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events),
                renderer=_Renderer(events),
                output_path=None,
                display=False,
                realtime_playback=False,
                max_frames=10,
                overwrite=False,
                max_seconds=0.04,
            )

        self.assertEqual(summary.frames_processed, 2)
        self.assertEqual(summary.termination_reason, "duration_limit")
        self.assertFalse(summary.completed_stream)
        self.assertTrue(capture.released)

    def test_actual_decoded_dimensions_override_capture_metadata(self) -> None:
        events: list[str] = []
        capture = _Capture(
            [np.zeros((10, 18, 3), dtype=np.uint8)],
            events,
        )

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

        self.assertEqual((summary.source_width, summary.source_height), (18, 10))
        self.assertTrue(capture.released)

    def test_oversized_actual_frame_is_rejected_despite_small_metadata(self) -> None:
        events: list[str] = []
        oversized = np.broadcast_to(
            np.zeros((1, 1, 3), dtype=np.uint8),
            (2161, 3840, 3),
        )
        capture = _Capture([oversized], events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaisesRegex(VideoProcessingError, "maximum pixels"):
                process_video(
                    input_path=self.input_path,
                    segmenter=_Segmenter(events),
                    renderer=_Renderer(events),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=1,
                    overwrite=False,
                )

        self.assertEqual(events, ["decode:0"])
        self.assertTrue(capture.released)

    def test_writer_is_created_lazily_from_actual_decoded_dimensions(self) -> None:
        events: list[str] = []
        capture = _Capture(
            [np.zeros((10, 18, 3), dtype=np.uint8)],
            events,
        )
        writers: list[_Writer] = []

        def create_writer(
            path: str,
            _codec: int,
            _fps: float,
            dimensions: tuple[int, int],
        ) -> _Writer:
            writer = _Writer(path, dimensions)
            writers.append(writer)
            return writer

        fake_cv2 = SimpleNamespace(
            CAP_PROP_FPS=1,
            VideoCapture=lambda _path: capture,
            VideoWriter_fourcc=lambda *_characters: 0,
            VideoWriter=create_writer,
        )
        output_path = self.input_path.with_name("outlined.mp4")

        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            summary = process_video(
                input_path=self.input_path,
                segmenter=_Segmenter(events),
                renderer=_Renderer(events),
                output_path=output_path,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=False,
            )

        self.assertEqual(len(writers), 1)
        self.assertEqual(writers[0].dimensions, (18, 10))
        self.assertEqual(writers[0].frames, [(10, 18, 3)])
        self.assertTrue(writers[0].released)
        self.assertTrue(output_path.exists())
        self.assertEqual(summary.output_path, str(output_path.resolve()))

    def test_decoded_frame_shape_change_is_rejected_before_inference(self) -> None:
        events: list[str] = []
        capture = _Capture(
            [
                np.zeros((12, 16, 3), dtype=np.uint8),
                np.zeros((13, 16, 3), dtype=np.uint8),
            ],
            events,
        )

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaisesRegex(VideoProcessingError, "dimensions changed"):
                process_video(
                    input_path=self.input_path,
                    segmenter=_Segmenter(events),
                    renderer=_Renderer(events),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=3,
                    overwrite=False,
                )

        self.assertEqual(
            events,
            ["decode:0", "infer:0", "render:0", "decode:1"],
        )
        self.assertTrue(capture.released)

    def test_failed_open_releases_capture(self) -> None:
        capture = _Capture([], [], opened=False)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaisesRegex(VideoProcessingError, "could not open"):
                process_video(
                    input_path=self.input_path,
                    segmenter=_Segmenter([]),
                    renderer=_Renderer([]),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=1,
                    overwrite=False,
                )

        self.assertTrue(capture.released)

    def test_inference_failure_releases_capture(self) -> None:
        events: list[str] = []
        capture = _Capture([np.zeros((12, 16, 3), dtype=np.uint8)], events)

        with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
            with self.assertRaisesRegex(RuntimeError, "fixture failed"):
                process_video(
                    input_path=self.input_path,
                    segmenter=_Segmenter(events, fail=True),
                    renderer=_Renderer(events),
                    output_path=None,
                    display=False,
                    realtime_playback=False,
                    max_frames=1,
                    overwrite=False,
                )

        self.assertTrue(capture.released)


if __name__ == "__main__":
    unittest.main()
