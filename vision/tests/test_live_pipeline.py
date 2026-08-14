"""Tests for the live inference pipeline config, runner, and hotkey rebinding."""

from __future__ import annotations

import json
import sys
import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.capture import CaptureConfig
from cs2_vision_access.inference.pipeline import (
    LivePipelineConfig,
    LiveRunSummary,
    run_live_pipeline,
)


class TestLivePipelineConfig(unittest.TestCase):
    """Verify LivePipelineConfig defaults."""

    def test_defaults(self) -> None:
        cfg = LivePipelineConfig()
        self.assertEqual(cfg.max_frames, 0)
        self.assertEqual(cfg.display_scale, 0.5)
        self.assertFalse(cfg.headless)
        self.assertEqual(cfg.alpha_output_mode, "overlay")
        self.assertIsInstance(cfg.capture, CaptureConfig)
        assert isinstance(cfg.capture, CaptureConfig)
        self.assertEqual(cfg.capture.source, 0)


class TestLiveRunSummary(unittest.TestCase):
    """Verify LiveRunSummary dataclass."""

    def test_create_summary(self) -> None:
        summary = LiveRunSummary(
            frames_processed=100,
            frames_inferred=80,
            frames_dropped=20,
            instances_predicted=45,
            instances_outlined=40,
            temporal_suppressed=3,
            inference_ms_p50=12.5,
            inference_ms_p95=25.0,
            pipeline_ms_p50=15.0,
            pipeline_ms_p95=30.0,
            elapsed_seconds=10.0,
            capture_fps=60.0,
            throughput_fps=10.0,
            termination_reason="user_stop",
        )
        self.assertEqual(summary.frames_processed, 100)
        self.assertEqual(summary.throughput_fps, 10.0)
        self.assertEqual(summary.temporal_suppressed, 3)

    def test_json_serializable(self) -> None:
        summary = LiveRunSummary(
            frames_processed=50,
            frames_inferred=40,
            frames_dropped=10,
            instances_predicted=20,
            instances_outlined=18,
            temporal_suppressed=0,
            inference_ms_p50=10.0,
            inference_ms_p95=20.0,
            pipeline_ms_p50=12.0,
            pipeline_ms_p95=25.0,
            elapsed_seconds=5.0,
            capture_fps=60.0,
            throughput_fps=10.0,
            termination_reason="frame_limit",
        )
        # Verify all numeric fields can be serialized
        d = {
            "frames_processed": summary.frames_processed,
            "frames_inferred": summary.frames_inferred,
            "frames_dropped": summary.frames_dropped,
            "instances_predicted": summary.instances_predicted,
            "instances_outlined": summary.instances_outlined,
            "temporal_suppressed": summary.temporal_suppressed,
            "inference_ms_p50": summary.inference_ms_p50,
            "inference_ms_p95": summary.inference_ms_p95,
            "pipeline_ms_p50": summary.pipeline_ms_p50,
            "pipeline_ms_p95": summary.pipeline_ms_p95,
            "elapsed_seconds": summary.elapsed_seconds,
            "capture_fps": summary.capture_fps,
            "throughput_fps": summary.throughput_fps,
            "termination_reason": summary.termination_reason,
        }
        payload = json.dumps(d)
        self.assertIn("frames_processed", payload)


class TestRunLivePipeline(unittest.TestCase):
    """Verify the live pipeline runs and returns a summary.

    Mocks OpenCV on every module that binds ``cv2`` at import time (or via
    lazy import). On Windows, an unmocked ``VideoCapture(0)`` can hang for
    minutes while DirectShow probes the camera.
    """

    def setUp(self) -> None:
        self.mock_segmenter = MagicMock()
        self.mock_segmenter.predict.return_value = ()
        self._cv2_patches: list[object] = []
        self._original_sys_cv2 = sys.modules.get("cv2")

    def tearDown(self) -> None:
        for p in self._cv2_patches:
            p.stop()
        self._cv2_patches.clear()
        if self._original_sys_cv2 is None:
            sys.modules.pop("cv2", None)
        else:
            sys.modules["cv2"] = self._original_sys_cv2

    def _inject_cv2(self) -> MagicMock:
        """Install a mock cv2 into sys.modules and live package bindings."""
        cv2 = MagicMock()
        cv2.CAP_PROP_FRAME_WIDTH = 3
        cv2.CAP_PROP_FRAME_HEIGHT = 4
        cv2.CAP_PROP_FPS = 5
        cv2.CAP_PROP_BACKEND = 6
        cv2.CAP_PROP_BUFFERSIZE = 7
        cv2.CAP_DSHOW = 700
        cv2.CAP_AVFOUNDATION = 1200
        cv2.CAP_MSMF = 1400
        cv2.CAP_ANY = 0
        cv2.CAP_V4L2 = 200
        cv2.WINDOW_NORMAL = 0
        cv2.COLOR_BGRA2BGR = 4
        cv2.COLOR_BGR2GRAY = 6
        cv2.COLOR_RGBA2BGR = 0
        cv2.COLOR_BGR2RGB = 4
        cv2.LINE_AA = 16
        cv2.FONT_HERSHEY_SIMPLEX = 0
        cv2.namedWindow = MagicMock()
        cv2.resizeWindow = MagicMock()
        cv2.imshow = MagicMock()
        cv2.waitKey = MagicMock(return_value=ord("q"))
        cv2.destroyWindow = MagicMock()
        cv2.destroyAllWindows = MagicMock()
        cv2.putText = MagicMock()
        cv2.cvtColor = MagicMock(side_effect=lambda img, code: img)
        cv2.GaussianBlur = MagicMock(side_effect=lambda img, *a, **k: img)
        cv2.contourArea = MagicMock(return_value=100.0)
        cv2.fillPoly = MagicMock()
        cv2.polylines = MagicMock()
        cv2.getStructuringElement = MagicMock(return_value=np.zeros((3, 3), dtype=np.uint8))
        cv2.dilate = MagicMock(side_effect=lambda img, *a, **k: img)
        cv2.VideoWriter_fourcc = MagicMock(return_value=0)
        cv2.VideoWriter = MagicMock()
        sys.modules["cv2"] = cv2
        # capture/alpha_renderer bind cv2 at import time — patch those too.
        for target in (
            "cs2_vision_access.capture.base.cv2",
            "cs2_vision_access.capture.alpha_renderer.cv2",
        ):
            p = patch(target, cv2)
            p.start()
            self._cv2_patches.append(p)
        return cv2

    def _make_cap_mock(self, width=640, height=480, fps=30.0) -> MagicMock:
        cap = MagicMock()
        cap.isOpened.return_value = True
        cap.get.side_effect = lambda prop: {  # noqa: ARG005
            3: width,
            4: height,
            5: fps,
        }.get(int(prop) if hasattr(prop, "__int__") else prop, 0.0)
        return cap

    def test_pipeline_runs_and_returns_summary(self) -> None:
        cv2 = self._inject_cv2()
        cap = self._make_cap_mock()
        read_results = [(True, np.zeros((480, 640, 3), dtype=np.uint8))] * 3 + [(False, None)]

        def read_side() -> tuple[bool, object]:
            return read_results.pop(0) if read_results else (False, None)

        cap.read.side_effect = read_side
        cv2.VideoCapture.return_value = cap

        cfg = LivePipelineConfig(
            max_frames=5,
            headless=False,
            capture=CaptureConfig(source=0),
        )
        summary = run_live_pipeline(
            segmenter=self.mock_segmenter,
            config=cfg,
        )
        self.assertIsInstance(summary, LiveRunSummary)
        self.assertGreaterEqual(summary.frames_processed, 0)

    def test_headless_mode_no_window(self) -> None:
        cv2 = self._inject_cv2()
        cap = self._make_cap_mock()
        read_results = [(True, np.zeros((480, 640, 3), dtype=np.uint8))] * 2 + [(False, None)]

        def read_side() -> tuple[bool, object]:
            return read_results.pop(0) if read_results else (False, None)

        cap.read.side_effect = read_side
        cv2.VideoCapture.return_value = cap

        cfg = LivePipelineConfig(headless=True, capture=CaptureConfig(source=0))
        summary = run_live_pipeline(
            segmenter=self.mock_segmenter,
            config=cfg,
        )
        self.assertIsInstance(summary, LiveRunSummary)
        cv2.namedWindow.assert_not_called()

    def test_pipeline_drops_frames_when_behind(self) -> None:
        """When inference takes too long, frames should be skipped."""
        cv2 = self._inject_cv2()
        cap = self._make_cap_mock(fps=60.0)
        # Bound the run so we do not depend on a fragile time side-effect length.
        read_results = [(True, np.zeros((480, 640, 3), dtype=np.uint8))] * 6 + [(False, None)]

        def read_side() -> tuple[bool, object]:
            return read_results.pop(0) if read_results else (False, None)

        cap.read.side_effect = read_side
        cv2.VideoCapture.return_value = cap

        def slow_predict(*args: object, **kwargs: object) -> tuple[()]:
            return ()

        self.mock_segmenter.predict.side_effect = slow_predict

        cfg = LivePipelineConfig(
            max_frames=4,
            frame_skip_threshold=2,
            headless=True,
            capture=CaptureConfig(source=0),
            diagnostic_interval=0.1,
        )
        counter = {"t": 0.0}

        def fake_perf_counter() -> float:
            # Advance enough between calls that the pipeline can skip work.
            counter["t"] += 0.05
            return counter["t"]

        mock_time_runner = patch("cs2_vision_access.inference.pipeline.runner.time")
        mock_time_frame = patch("cs2_vision_access.inference.pipeline.frame.time")
        with mock_time_runner as mock_tr, mock_time_frame as mock_tf:
            mock_tr.perf_counter.side_effect = fake_perf_counter
            mock_tr.sleep = MagicMock()
            mock_tf.perf_counter.side_effect = fake_perf_counter
            summary = run_live_pipeline(
                segmenter=self.mock_segmenter,
                config=cfg,
            )
            self.assertIsInstance(summary, LiveRunSummary)


class TestHotkeyControlStateRetention(unittest.TestCase):
    """Hotkey toggles must rebind renderer/temporal_policy and stick across frames."""

    def _frame_helpers(self):
        from cs2_vision_access.capture.hotkeys import LiveControlState
        from cs2_vision_access.inference.pipeline.config import LivePipelineConfig
        from cs2_vision_access.inference.pipeline.diagnostics import _Diagnostics
        from cs2_vision_access.inference.pipeline.frame import run_frame
        from cs2_vision_access.renderer import OutlineRenderer, OutlineStyle

        frame = np.zeros((48, 64, 3), dtype=np.uint8)
        capture_mgr = MagicMock()
        capture_mgr.read.return_value = (True, frame, None)
        segmenter = MagicMock()
        segmenter.predict.return_value = ()
        display_mgr = MagicMock()
        display_mgr.show_frame.return_value = True
        cfg = LivePipelineConfig(headless=True, frame_skip_threshold=10_000)
        base_style = OutlineStyle()
        return {
            "run_frame": run_frame,
            "LiveControlState": LiveControlState,
            "capture_mgr": capture_mgr,
            "segmenter": segmenter,
            "display_mgr": display_mgr,
            "cfg": cfg,
            "base_style": base_style,
            "OutlineRenderer": OutlineRenderer,
            "diag": _Diagnostics(),
            "started": __import__("time").perf_counter(),
        }

    def test_run_frame_returns_rebound_temporal_policy(self) -> None:
        h = self._frame_helpers()
        control = h["LiveControlState"](temporal_enabled=True)
        initial_renderer = h["OutlineRenderer"](style=h["base_style"])

        result = h["run_frame"](
            frame_index=0,
            capture_mgr=h["capture_mgr"],
            segmenter=h["segmenter"],
            cfg=h["cfg"],
            capture_fps=30.0,
            started=h["started"],
            base_outline_style=h["base_style"],
            renderer=initial_renderer,
            temporal_policy=None,
            control_state=control,
            display_mgr=h["display_mgr"],
            on_frame=None,
            diag=h["diag"],
            last_temporal_enabled=False,
            last_output_mode="overlay",
            last_alpha_fill=False,
        )

        self.assertTrue(result.last_temporal_enabled)
        self.assertIsNotNone(result.temporal_policy)
        from cs2_vision_access.inference.temporal import SuppressOnlyTemporalPolicy

        self.assertIsInstance(result.temporal_policy, SuppressOnlyTemporalPolicy)

    def test_run_frame_returns_rebound_renderer_on_mode_change(self) -> None:
        h = self._frame_helpers()
        from cs2_vision_access.capture.alpha_renderer import AlphaRenderer

        control = h["LiveControlState"](output_mode="alpha", alpha_fill=False)
        initial_renderer = h["OutlineRenderer"](style=h["base_style"])

        result = h["run_frame"](
            frame_index=0,
            capture_mgr=h["capture_mgr"],
            segmenter=h["segmenter"],
            cfg=h["cfg"],
            capture_fps=30.0,
            started=h["started"],
            base_outline_style=h["base_style"],
            renderer=initial_renderer,
            temporal_policy=None,
            control_state=control,
            display_mgr=h["display_mgr"],
            on_frame=None,
            diag=h["diag"],
            last_temporal_enabled=False,
            last_output_mode="overlay",
            last_alpha_fill=False,
        )

        self.assertEqual(result.last_output_mode, "alpha")
        self.assertIsInstance(result.renderer, AlphaRenderer)
        self.assertIsNot(result.renderer, initial_renderer)

    def test_runner_retains_temporal_policy_and_renderer_across_frames(self) -> None:
        """Runner must pass rebound objects into the next run_frame call."""
        from cs2_vision_access.capture import CaptureConfig
        from cs2_vision_access.inference.pipeline import runner as runner_mod
        from cs2_vision_access.inference.pipeline.config import LivePipelineConfig
        from cs2_vision_access.inference.pipeline.frame import _FrameResult

        cv2 = MagicMock()
        cv2.CAP_PROP_FRAME_WIDTH = 3
        cv2.CAP_PROP_FRAME_HEIGHT = 4
        cv2.CAP_PROP_FPS = 5
        cv2.CAP_PROP_BACKEND = 6
        cv2.CAP_PROP_BUFFERSIZE = 7
        cv2.CAP_DSHOW = 700
        cv2.CAP_AVFOUNDATION = 1200
        cv2.CAP_MSMF = 1400
        cv2.CAP_ANY = 0
        cv2.CAP_V4L2 = 200
        cv2.WINDOW_NORMAL = 0

        cap = MagicMock()
        cap.isOpened.return_value = True
        cap.get.side_effect = lambda prop: {3: 64, 4: 48, 5: 30.0}.get(
            int(prop) if hasattr(prop, "__int__") else prop, 0.0
        )
        cap.read.return_value = (True, np.zeros((48, 64, 3), dtype=np.uint8))
        cv2.VideoCapture.return_value = cap

        original_renderer = MagicMock(name="original_renderer")
        rebound_renderer = MagicMock(name="rebound_renderer")
        rebound_policy = MagicMock(name="rebound_policy")
        seen_kwargs: list[dict] = []

        def fake_run_frame(**kwargs):
            seen_kwargs.append(kwargs)
            n = len(seen_kwargs)
            if n == 1:
                return _FrameResult(
                    frames_inferred_delta=1,
                    last_temporal_enabled=True,
                    last_output_mode="alpha",
                    last_alpha_fill=False,
                    renderer=rebound_renderer,
                    temporal_policy=rebound_policy,
                )
            return _FrameResult(
                frames_inferred_delta=1,
                last_temporal_enabled=True,
                last_output_mode="alpha",
                last_alpha_fill=False,
                renderer=kwargs["renderer"],
                temporal_policy=kwargs["temporal_policy"],
            )

        segmenter = MagicMock()
        segmenter.predict.return_value = ()
        cfg = LivePipelineConfig(
            max_frames=2,
            headless=True,
            capture=CaptureConfig(source=0),
        )

        with (
            patch.object(runner_mod, "run_frame", side_effect=fake_run_frame),
            patch.dict("sys.modules", {"cv2": cv2}),
            patch("cs2_vision_access.capture.base.cv2", cv2),
        ):
            summary = runner_mod.run_live_pipeline(
                segmenter=segmenter,
                config=cfg,
                renderer=original_renderer,
            )

        self.assertEqual(summary.frames_processed, 2)
        self.assertEqual(len(seen_kwargs), 2)
        # First frame starts with the pipeline's initial objects.
        self.assertIs(seen_kwargs[0]["renderer"], original_renderer)
        self.assertIsNone(seen_kwargs[0]["temporal_policy"])
        # Second frame must receive the rebound objects from frame 1.
        self.assertIs(seen_kwargs[1]["renderer"], rebound_renderer)
        self.assertIs(seen_kwargs[1]["temporal_policy"], rebound_policy)
        self.assertTrue(seen_kwargs[1]["last_temporal_enabled"])
        self.assertEqual(seen_kwargs[1]["last_output_mode"], "alpha")


if __name__ == "__main__":
    unittest.main()
