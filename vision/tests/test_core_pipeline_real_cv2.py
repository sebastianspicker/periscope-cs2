"""Core frame → instance masks → dual-stroke outline path with real OpenCV.

These tests drive the shipped ``OutlineRenderer`` and ``process_video`` entry
points without mocking OpenCV or re-implementing stroke drawing. They assert
measurable geometric/pixel outcomes: valid polygons, non-empty masks, and
rendered frames that differ from the input where dual-stroke outlines land.
"""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import OutlineRenderer, OutlineStyle
from cs2_vision_access.video import process_video


def _player_mask(frame_index: int = 0) -> InstanceMask:
    """Axis-aligned rectangle large enough for dual-stroke widths to land."""
    return InstanceMask(
        frame_index=frame_index,
        polygon=((12.0, 12.0), (52.0, 12.0), (52.0, 52.0), (12.0, 52.0)),
        confidence=0.91,
        class_id=0,
        class_name="player",
    )


class RealCv2DualStrokeRenderTests(unittest.TestCase):
    """Ship-path dual-stroke rendering with the real OpenCV backend."""

    def test_render_differs_from_input_and_reports_contour(self) -> None:
        import cv2  # real dependency — not mocked

        frame = np.full((64, 64, 3), 40, dtype=np.uint8)
        mask = _player_mask()
        style = OutlineStyle(fill_opacity=0.0, scale_with_frame=False)
        renderer = OutlineRenderer(style)

        rendered, diagnostics = renderer.render_with_diagnostics(frame, (mask,), frame_index=0)

        self.assertEqual(rendered.shape, frame.shape)
        self.assertEqual(rendered.dtype, frame.dtype)
        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(diagnostics.degenerate_masks_discarded, 0)
        self.assertEqual(diagnostics.stale_predictions_discarded, 0)
        self.assertGreater(diagnostics.outer_width_pixels, diagnostics.inner_width_pixels)

        # Measurable pixel change: outline strokes are not identity.
        diff_mask = np.any(rendered != frame, axis=2)
        changed = int(np.count_nonzero(diff_mask))
        self.assertGreater(changed, 0)

        # Dual-stroke: both outer (dark) and inner (bright) colours appear.
        from cs2_vision_access.renderer import parse_hex_bgr

        outer_bgr = np.asarray(parse_hex_bgr(style.outer_color), dtype=np.uint8)
        inner_bgr = np.asarray(parse_hex_bgr(style.inner_color), dtype=np.uint8)
        outer_hits = int(np.count_nonzero(np.all(rendered == outer_bgr, axis=2)))
        inner_hits = int(np.count_nonzero(np.all(rendered == inner_bgr, axis=2)))
        self.assertGreater(outer_hits, 0, "outer dual-stroke colour missing from render")
        self.assertGreater(inner_hits, 0, "inner dual-stroke colour missing from render")

        # Deterministic: second render matches first.
        again = renderer.render(frame, (mask,), frame_index=0)
        self.assertTrue(np.array_equal(rendered, again))

        # Sanity: OpenCV contour area of the polygon is positive (mask geometry).
        pts = np.asarray(mask.polygon, dtype=np.float32).reshape((-1, 1, 2))
        self.assertGreater(abs(float(cv2.contourArea(pts))), 0.0)

    def test_stale_and_degenerate_masks_do_not_paint(self) -> None:
        frame = np.full((48, 48, 3), 30, dtype=np.uint8)
        valid = _player_mask(frame_index=2)
        stale = InstanceMask(
            frame_index=1,
            polygon=valid.polygon,
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        degenerate = InstanceMask(
            frame_index=2,
            polygon=((1.0, 1.0), (2.0, 1.0), (3.0, 1.0)),  # zero area line
            confidence=0.9,
            class_id=0,
            class_name="player",
        )
        style = OutlineStyle(fill_opacity=0.0, scale_with_frame=False)
        rendered, diagnostics = OutlineRenderer(style).render_with_diagnostics(
            frame, (valid, stale, degenerate), frame_index=2
        )
        self.assertEqual(diagnostics.contours_rendered, 1)
        self.assertEqual(diagnostics.stale_predictions_discarded, 1)
        self.assertEqual(diagnostics.degenerate_masks_discarded, 1)
        self.assertGreater(int(np.count_nonzero(rendered != frame)), 0)


class _FixedPolygonSegmenter:
    """Minimal Segmenter protocol: one valid player mask per frame."""

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        del frame_bgr
        return (_player_mask(frame_index=frame_index),)


class RealCv2ProcessVideoTests(unittest.TestCase):
    """Offline ``process_video`` with real OpenCV encode/decode and shipped renderer."""

    def _write_tiny_video(self, path: Path, *, frames: int = 3, size: int = 64) -> None:
        import cv2

        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        writer = cv2.VideoWriter(str(path), fourcc, 10.0, (size, size))
        self.assertTrue(writer.isOpened(), "VideoWriter failed to open")
        try:
            for i in range(frames):
                # Distinct background per frame so identity leaks would show.
                frame = np.full((size, size, 3), 20 + i * 5, dtype=np.uint8)
                writer.write(frame)
        finally:
            writer.release()

    def test_process_video_writes_outlined_output_twice_consistently(self) -> None:
        import cv2

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "synthetic.mp4"
            out_a = root / "outlined_a.mp4"
            out_b = root / "outlined_b.mp4"
            self._write_tiny_video(source, frames=3, size=64)

            style = OutlineStyle(fill_opacity=0.0, scale_with_frame=False)
            renderer = OutlineRenderer(style)
            segmenter = _FixedPolygonSegmenter()

            summaries = []
            for destination in (out_a, out_b):
                summary = process_video(
                    input_path=source,
                    segmenter=segmenter,
                    renderer=renderer,
                    output_path=destination,
                    display=False,
                    realtime_playback=False,
                    max_frames=3,
                    overwrite=True,
                )
                summaries.append(summary)
                self.assertTrue(destination.is_file())
                self.assertGreater(destination.stat().st_size, 0)
                self.assertEqual(summary.frames_processed, 3)
                self.assertEqual(summary.instances_predicted, 3)
                self.assertEqual(summary.instances_outlined, 3)

            # Both launches report the same primary counters.
            self.assertEqual(summaries[0].frames_processed, summaries[1].frames_processed)
            self.assertEqual(summaries[0].instances_outlined, summaries[1].instances_outlined)

            # Decode first output frame and confirm it differs from a plain source frame.
            cap_src = cv2.VideoCapture(str(source))
            cap_out = cv2.VideoCapture(str(out_a))
            try:
                ok_s, src_frame = cap_src.read()
                ok_o, out_frame = cap_out.read()
            finally:
                cap_src.release()
                cap_out.release()
            self.assertTrue(ok_s and ok_o)
            self.assertEqual(src_frame.shape, out_frame.shape)
            changed = int(np.count_nonzero(out_frame != src_frame))
            self.assertGreater(
                changed,
                0,
                "outlined video frame is identical to source — dual-stroke did not land",
            )


if __name__ == "__main__":
    unittest.main()
