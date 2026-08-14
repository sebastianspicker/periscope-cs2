"""Tests for the live capture, alpha rendering, and pipeline modules.

These tests verify the capture module logic, alpha renderer, and pipeline
config without requiring actual capture hardware or installed dependencies.
OpenCV, Ultralytics, and supervision are all mocked/stubbed.
"""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.capture import (
    AlphaRenderer,
    AlphaRendererConfig,
    CaptureConfig,
    list_capture_devices,
    open_capture,
)
from cs2_vision_access.capture.alpha_renderer import ALPHA_OUTPUT_MODES
from cs2_vision_access.capture.base import (
    CaptureDeviceInfo,
    FrameTimingDiagnostics,
    LiveCaptureError,
)
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer import OutlineStyle


class TestCaptureDeviceInfo(unittest.TestCase):
    """Verify CaptureDeviceInfo dataclass."""

    def test_create_device_info(self) -> None:
        info = CaptureDeviceInfo(index=0, name="Magewell Capture", backend="auto")
        self.assertEqual(info.index, 0)
        self.assertEqual(info.name, "Magewell Capture")


class TestCaptureConfig(unittest.TestCase):
    """Verify CaptureConfig defaults and construction."""

    def test_defaults(self) -> None:
        cfg = CaptureConfig()
        self.assertEqual(cfg.source, 0)
        self.assertEqual(cfg.preferred_width, 1920)
        self.assertEqual(cfg.preferred_height, 1080)
        self.assertEqual(cfg.preferred_fps, 60.0)

    def test_integer_source(self) -> None:
        cfg = CaptureConfig(source=2)
        self.assertEqual(cfg.source, 2)

    def test_string_source(self) -> None:
        cfg = CaptureConfig(source="Elgato HD60")
        self.assertEqual(cfg.source, "Elgato HD60")


class TestListCaptureDevices(unittest.TestCase):
    """Verify device enumeration."""

    @patch("cs2_vision_access.capture.base.cv2")
    def test_no_devices(self, mock_cv2: MagicMock) -> None:
        cap_instance = MagicMock()
        cap_instance.isOpened.return_value = False
        mock_cv2.VideoCapture.return_value = cap_instance
        mock_cv2.CAP_DSHOW = 700
        mock_cv2.CAP_AVFOUNDATION = 1200
        devices = list_capture_devices(max_devices=3)
        self.assertEqual(devices, [])


@patch("cs2_vision_access.capture.base.cv2")
class TestOpenCapture(unittest.TestCase):
    """Verify capture device opening."""

    def test_open_by_index(self, mock_cv2: MagicMock) -> None:
        cap_instance = MagicMock()
        cap_instance.isOpened.return_value = True
        mock_cv2.VideoCapture.return_value = cap_instance
        mock_cv2.CAP_DSHOW = 700
        mock_cv2.CAP_AVFOUNDATION = 1200
        mock_cv2.CAP_MSMF = 1400
        mock_cv2.CAP_ANY = 0
        mock_cv2.CAP_V4L2 = 200
        mock_cv2.CAP_PROP_BUFFERSIZE = 38
        mock_cv2.CAP_PROP_FRAME_WIDTH = 3
        mock_cv2.CAP_PROP_FRAME_HEIGHT = 4
        mock_cv2.CAP_PROP_FPS = 5

        cfg = CaptureConfig(source=0)
        result = open_capture(cfg)
        self.assertEqual(result, cap_instance)

    def test_open_by_index_failure(self, mock_cv2: MagicMock) -> None:
        cap_instance = MagicMock()
        cap_instance.isOpened.return_value = False
        mock_cv2.VideoCapture.return_value = cap_instance
        mock_cv2.CAP_DSHOW = 700
        mock_cv2.CAP_AVFOUNDATION = 1200
        mock_cv2.CAP_MSMF = 1400
        mock_cv2.CAP_ANY = 0
        mock_cv2.CAP_V4L2 = 200

        cfg = CaptureConfig(source=99)
        with self.assertRaises(LiveCaptureError):
            open_capture(cfg)

    def test_open_file_path(self, mock_cv2: MagicMock) -> None:
        """Opening a local file path should succeed."""
        cap_instance = MagicMock()
        cap_instance.isOpened.return_value = True
        mock_cv2.VideoCapture.return_value = cap_instance
        mock_cv2.CAP_DSHOW = 700
        mock_cv2.CAP_AVFOUNDATION = 1200
        mock_cv2.CAP_MSMF = 1400
        mock_cv2.CAP_ANY = 0
        mock_cv2.CAP_V4L2 = 200

        with patch("pathlib.Path.is_file", return_value=True):
            cfg = CaptureConfig(source="/tmp/test.mp4")
            result = open_capture(cfg)
            self.assertEqual(result, cap_instance)


class TestFrameTimingDiagnostics(unittest.TestCase):
    """Verify frame timing tracking."""

    def test_initial_state(self) -> None:
        timing = FrameTimingDiagnostics()
        self.assertEqual(timing.frames_received, 0)
        self.assertEqual(timing.frames_dropped, 0)

    def test_record_one_frame(self) -> None:
        timing = FrameTimingDiagnostics()
        timing.record_received()
        self.assertEqual(timing.frames_received, 1)

    def test_record_multiple_frames(self) -> None:
        timing = FrameTimingDiagnostics()
        timing.record_received()
        timing.record_received()
        timing.record_received()
        self.assertEqual(timing.frames_received, 3)


class TestAlphaRendererConfig(unittest.TestCase):
    """Verify AlphaRendererConfig defaults and modes."""

    def test_defaults(self) -> None:
        cfg = AlphaRendererConfig()
        self.assertEqual(cfg.output_mode, "alpha")
        self.assertFalse(cfg.enable_fill)

    def test_valid_modes(self) -> None:
        self.assertIn("alpha", ALPHA_OUTPUT_MODES)
        self.assertIn("green", ALPHA_OUTPUT_MODES)
        self.assertIn("overlay", ALPHA_OUTPUT_MODES)


@patch("cs2_vision_access.capture.alpha_renderer.cv2")
class TestAlphaRenderer(unittest.TestCase):
    """Verify AlphaRenderer produces correctly shaped output."""

    def setUp(self) -> None:
        self.style = OutlineStyle()
        self.predictions = (
            InstanceMask(
                frame_index=0,
                polygon=((10, 10), (100, 10), (100, 100), (10, 100)),
                confidence=0.95,
                class_id=0,
                class_name="player",
            ),
        )

    def _configure_cv2(self, mock_cv2: MagicMock) -> None:
        mock_cv2.COLOR_BGR2GRAY = 6
        mock_cv2.COLOR_RGBA2BGR = 0
        mock_cv2.COLOR_BGR2RGB = 4
        mock_cv2.LINE_AA = 16

        def fake_cvt_color(img: np.ndarray, code: int) -> np.ndarray:
            arr = np.asarray(img)
            if code == mock_cv2.COLOR_BGR2GRAY:
                if arr.ndim == 3:
                    return arr.mean(axis=2).astype(np.uint8)
                return arr.astype(np.uint8)
            # COLOR_BGR2RGB / other 3-channel conversions keep shape.
            return arr.copy() if arr.ndim == 3 else np.stack([arr, arr, arr], axis=-1)

        mock_cv2.cvtColor.side_effect = fake_cvt_color
        mock_cv2.GaussianBlur.side_effect = lambda img, *a, **k: img
        mock_cv2.contourArea.return_value = 100.0
        mock_cv2.fillPoly = MagicMock()
        mock_cv2.polylines = MagicMock()
        mock_cv2.getStructuringElement.return_value = np.zeros((3, 3), dtype=np.uint8)
        mock_cv2.dilate.side_effect = lambda img, *a, **k: img

    def test_alpha_mode_output_shape(self, mock_cv2: MagicMock) -> None:
        self._configure_cv2(mock_cv2)

        renderer = AlphaRenderer(
            style=self.style,
            alpha_config=AlphaRendererConfig(output_mode="alpha"),
        )
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        result = renderer.render(frame, self.predictions, frame_index=0)
        self.assertEqual(result.shape, (480, 640, 4))  # RGBA

    def test_green_mode_output_shape(self, mock_cv2: MagicMock) -> None:
        self._configure_cv2(mock_cv2)

        renderer = AlphaRenderer(
            style=self.style,
            alpha_config=AlphaRendererConfig(output_mode="green"),
        )
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        result = renderer.render(frame, self.predictions, frame_index=0)
        self.assertEqual(result.shape, (480, 640, 3))  # BGR

    def test_overlay_mode_shape(self, mock_cv2: MagicMock) -> None:
        self._configure_cv2(mock_cv2)
        renderer = AlphaRenderer(
            style=self.style,
            alpha_config=AlphaRendererConfig(output_mode="overlay"),
        )
        frame = np.ones((480, 640, 3), dtype=np.uint8) * 128
        result = renderer.render(frame, self.predictions, frame_index=0)
        self.assertEqual(result.shape, (480, 640, 3))  # BGR

    def test_empty_predictions_passthrough(self, mock_cv2: MagicMock) -> None:
        self._configure_cv2(mock_cv2)
        renderer = AlphaRenderer(
            style=self.style,
            alpha_config=AlphaRendererConfig(output_mode="alpha"),
        )
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        result = renderer.render(frame, [], frame_index=0)
        # No outlines → empty RGBA
        self.assertEqual(result.shape, (480, 640, 4))
        self.assertEqual(result[:, :, 3].sum(), 0)  # Alpha is all zero


if __name__ == "__main__":
    unittest.main()
