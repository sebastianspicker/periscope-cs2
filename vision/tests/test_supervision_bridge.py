"""Tests for the Supervision integration bridge.

These tests verify the conversion logic between InstanceMask and sv.Detections.
The actual supervision package is mocked so tests stay dependency-light.
"""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.inference.supervision import (
    SupervisionAnnotator,
    SupervisionAnnotatorConfig,
    SupervisionBridgeError,
    detections_to_instance_masks,
    instance_masks_to_detections,
)
from cs2_vision_access.predictions import InstanceMask


class MockDetections:
    """Minimal mock for sv.Detections to test conversion logic."""

    def __init__(self, n: int = 0) -> None:
        self._n = n
        self.xyxy = np.array([[10, 20, 100, 200]], dtype=np.float32) if n > 0 else np.empty((0, 4))
        self.mask = (
            np.array([[[True, False], [False, True]]], dtype=bool)
            if n > 0
            else np.empty((0, 0, 0), dtype=bool)
        )
        self.confidence = (
            np.array([0.95], dtype=np.float32) if n > 0 else np.empty((0,), dtype=np.float32)
        )
        self.class_id = np.array([0], dtype=int) if n > 0 else np.empty((0,), dtype=int)
        self.data: dict[str, object] = (
            {"class_name": np.array(["player"], dtype=object)} if n > 0 else {}
        )

    def __len__(self) -> int:
        return self._n


class TestInstanceMasksToDetections(unittest.TestCase):
    """Verify conversion from InstanceMask to sv.Detections."""

    def test_empty_masks_returns_empty(self) -> None:
        """Empty mask sequence should yield an empty Detections object."""
        with (
            patch("cs2_vision_access.renderer.supervision.sv") as mock_sv,
        ):
            mock_sv.Detections.empty.return_value = "EMPTY"
            result = instance_masks_to_detections([], with_labels=True)
            self.assertEqual(result, "EMPTY")
            mock_sv.Detections.empty.assert_called_once()

    def test_single_mask_produces_valid_detections(self) -> None:
        """A single InstanceMask should produce a Detections with matching fields."""
        mask = InstanceMask(
            frame_index=0,
            polygon=((10.0, 20.0), (100.0, 20.0), (100.0, 200.0), (10.0, 200.0)),
            confidence=0.95,
            class_id=0,
            class_name="player",
        )
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        with (
            patch("cs2_vision_access.renderer.supervision.sv") as mock_sv,
            patch("cs2_vision_access.renderer.supervision.cv2") as mock_cv2,
        ):
            mock_cv2.fillPoly = MagicMock()
            mock_detections = MagicMock()
            mock_sv.Detections.return_value = mock_detections

            instance_masks_to_detections([mask], frame=frame)

            mock_sv.Detections.assert_called_once()
            call_kwargs = mock_sv.Detections.call_args[1]
            self.assertEqual(call_kwargs["class_id"].tolist(), [0])
            # float32 can print as 0.949999988079071 — compare numerically.
            np.testing.assert_allclose(call_kwargs["confidence"], [0.95], rtol=1e-5)
            self.assertIn("class_name", call_kwargs["data"])
            self.assertIn("label", call_kwargs["data"])

    def test_missing_supervision_raises_error(self) -> None:
        """When supervision is not installed, raise SupervisionBridgeError."""
        with patch("cs2_vision_access.renderer.supervision.sv", None):
            with self.assertRaises(SupervisionBridgeError):
                instance_masks_to_detections(
                    [
                        InstanceMask(
                            frame_index=0,
                            polygon=((0, 0), (1, 0), (1, 1), (0, 1)),
                            confidence=0.5,
                            class_id=0,
                            class_name="test",
                        )
                    ],
                )


class TestDetectionsToInstanceMasks(unittest.TestCase):
    """Verify round-trip from sv.Detections back to InstanceMask."""

    def test_empty_detections_returns_empty_tuple(self) -> None:
        with patch("cs2_vision_access.renderer.supervision.sv") as mock_sv:
            # isinstance(detections, sv.Detections) requires a real type here.
            mock_sv.Detections = MockDetections
            result = detections_to_instance_masks(MockDetections(0), frame_index=0)
            self.assertEqual(result, ())

    def test_detections_to_instances_with_mask(self) -> None:
        """Detections with masks should produce valid InstanceMask objects."""
        with (
            patch("cs2_vision_access.renderer.supervision.sv") as mock_sv,
            patch("cs2_vision_access.renderer.supervision.cv2") as mock_cv2,
        ):
            mock_sv.Detections = MockDetections
            mock_cv2.findContours.return_value = (
                [np.array([[[0, 0], [1, 0], [0, 1]]], dtype=np.int32)],
                None,
            )
            mock_cv2.RETR_EXTERNAL = 0
            mock_cv2.CHAIN_APPROX_SIMPLE = 1

            dets = MockDetections(n=1)
            results = detections_to_instance_masks(
                dets,
                frame_index=5,
                manifest_classes={0: "player"},
            )
            self.assertEqual(len(results), 1)
            self.assertEqual(results[0].frame_index, 5)
            self.assertEqual(results[0].class_name, "player")

    def test_detections_without_mask_uses_xyxy_box(self) -> None:
        """When no mask is available, fall back to xyxy bounding box polygon."""
        with (
            patch("cs2_vision_access.renderer.supervision.sv") as mock_sv,
        ):
            mock_sv.Detections = MockDetections

            class NoMaskDetections(MockDetections):
                def __init__(self) -> None:
                    super().__init__(n=1)
                    self.mask = None  # type: ignore[assignment]

            results = detections_to_instance_masks(
                NoMaskDetections(),
                frame_index=0,
                manifest_classes={0: "player"},
            )
            self.assertEqual(len(results), 1)
            # Should produce a 4-corner rectangle from xyxy
            polygon = results[0].polygon
            self.assertEqual(len(polygon), 4)


class TestSupervisionAnnotator(unittest.TestCase):
    """Verify SupervisionAnnotator construction and render dispatch."""

    def test_init_without_supervision_raises(self) -> None:
        with patch("cs2_vision_access.renderer.supervision.sv", None):
            with self.assertRaises(SupervisionBridgeError):
                SupervisionAnnotator()

    def test_init_with_supervision_mock_succeeds(self) -> None:
        with patch("cs2_vision_access.renderer.supervision.sv") as mock_sv:
            # Use INDEX by default
            mock_sv.ColorLookup.INDEX = "INDEX"
            annotator = SupervisionAnnotator()
            self.assertIsNotNone(annotator)

    def test_render_empty_predictions(self) -> None:
        with patch("cs2_vision_access.renderer.supervision.sv") as mock_sv:
            mock_sv.ColorLookup.INDEX = "INDEX"
            frame = np.zeros((100, 100, 3), dtype=np.uint8)
            annotator = SupervisionAnnotator()
            result = annotator.render(frame, [], frame_index=0)
            np.testing.assert_array_equal(result, frame)

    def test_annotator_setup_respects_config(self) -> None:
        """Annotator should only initialize annotators specified in config."""
        with patch("cs2_vision_access.renderer.supervision.sv") as mock_sv:
            mock_sv.ColorLookup.INDEX = "INDEX"
            mock_sv.BoxAnnotator = MagicMock(return_value="BOX")
            mock_sv.MaskAnnotator = MagicMock(return_value="MASK")

            config = SupervisionAnnotatorConfig(box=True, mask=False)
            annotator = SupervisionAnnotator(config=config)
            self.assertEqual(len(annotator._annotators), 1)
            self.assertEqual(annotator._annotators[0], "BOX")


if __name__ == "__main__":
    unittest.main()
