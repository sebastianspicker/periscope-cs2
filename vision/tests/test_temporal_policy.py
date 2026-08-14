from __future__ import annotations

import unittest

from cs2_vision_access.inference.temporal import (
    SuppressOnlyTemporalPolicy,
    TemporalDiagnostics,
    TemporalStabilityConfig,
)
from cs2_vision_access.predictions import InstanceMask


def _mask(
    frame_index: int,
    polygon: tuple[tuple[float, float], ...],
    *,
    class_id: int = 0,
    class_name: str = "player",
    confidence: float = 0.9,
) -> InstanceMask:
    return InstanceMask(
        frame_index=frame_index,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=class_name,
    )


SQUARE = ((1.0, 1.0), (5.0, 1.0), (5.0, 5.0), (1.0, 5.0))
SQUARE_SHIFTED = ((1.5, 1.2), (5.5, 1.2), (5.5, 5.2), (1.5, 5.2))
OTHER = ((20.0, 20.0), (30.0, 20.0), (30.0, 30.0), (20.0, 30.0))


class TemporalStabilityConfigTests(unittest.TestCase):
    def test_defaults_are_disabled(self) -> None:
        config = TemporalStabilityConfig()
        self.assertFalse(config.enabled)
        self.assertEqual(config.min_consecutive_frames, 2)

    def test_min_frames_must_be_positive_int(self) -> None:
        with self.assertRaises(ValueError):
            TemporalStabilityConfig(min_consecutive_frames=0)
        with self.assertRaises(ValueError):
            TemporalStabilityConfig(min_consecutive_frames=True)  # type: ignore[arg-type]


class SuppressOnlyTemporalPolicyTests(unittest.TestCase):
    def test_disabled_is_identity(self) -> None:
        policy = SuppressOnlyTemporalPolicy(TemporalStabilityConfig(enabled=False))
        masks = (
            _mask(0, SQUARE),
            _mask(0, OTHER, class_id=1, class_name="other"),
        )
        out, diagnostics = policy.filter(masks, frame_index=0)
        self.assertEqual(out, masks)
        self.assertIs(out[0], masks[0])
        self.assertEqual(
            diagnostics,
            TemporalDiagnostics(suppressed_count=0, passed_count=2),
        )
        # Still identity on subsequent frames when disabled.
        out2, diagnostics2 = policy.filter(masks, frame_index=1)
        self.assertEqual(out2, masks)
        self.assertEqual(diagnostics2.suppressed_count, 0)
        self.assertEqual(diagnostics2.passed_count, 2)

    def test_single_frame_blip_is_suppressed(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        blip = (_mask(0, SQUARE),)
        out0, diag0 = policy.filter(blip, frame_index=0)
        self.assertEqual(out0, ())
        self.assertEqual(diag0.suppressed_count, 1)
        self.assertEqual(diag0.passed_count, 0)

        # Dropout: no prediction — must not invent geometry.
        out1, diag1 = policy.filter((), frame_index=1)
        self.assertEqual(out1, ())
        self.assertEqual(diag1.suppressed_count, 0)
        self.assertEqual(diag1.passed_count, 0)

        # Reappearance after gap restarts the consecutive counter.
        out2, diag2 = policy.filter((_mask(2, SQUARE),), frame_index=2)
        self.assertEqual(out2, ())
        self.assertEqual(diag2.suppressed_count, 1)

    def test_stable_detection_passes_after_threshold(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        out0, diag0 = policy.filter((_mask(0, SQUARE),), frame_index=0)
        self.assertEqual(out0, ())
        self.assertEqual(diag0.suppressed_count, 1)

        # Small motion still associates via IoU/centroid.
        second = _mask(1, SQUARE_SHIFTED)
        out1, diag1 = policy.filter((second,), frame_index=1)
        self.assertEqual(len(out1), 1)
        self.assertIs(out1[0], second)
        self.assertEqual(diag1.suppressed_count, 0)
        self.assertEqual(diag1.passed_count, 1)

        third = _mask(2, SQUARE)
        out2, diag2 = policy.filter((third,), frame_index=2)
        self.assertEqual(len(out2), 1)
        self.assertIs(out2[0], third)
        self.assertEqual(diag2.passed_count, 1)

    def test_never_emits_wrong_frame_index(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        for frame_index in range(4):
            mask = _mask(frame_index, SQUARE)
            out, _ = policy.filter((mask,), frame_index=frame_index)
            for passed in out:
                self.assertEqual(passed.frame_index, frame_index)
                # Geometry is the detector output, not a held prior frame.
                self.assertEqual(passed.polygon, SQUARE)

    def test_dropout_never_holds_last_mask(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        policy.filter((_mask(1, SQUARE),), frame_index=1)
        # Stable for two frames; next frame detector empty → nothing drawn.
        out, diag = policy.filter((), frame_index=2)
        self.assertEqual(out, ())
        self.assertEqual(diag.passed_count, 0)
        self.assertEqual(diag.suppressed_count, 0)

    def test_min_consecutive_three(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=3)
        )
        for frame_index in range(2):
            out, diag = policy.filter(
                (_mask(frame_index, SQUARE),),
                frame_index=frame_index,
            )
            self.assertEqual(out, ())
            self.assertEqual(diag.suppressed_count, 1)
        out2, diag2 = policy.filter((_mask(2, SQUARE),), frame_index=2)
        self.assertEqual(len(out2), 1)
        self.assertEqual(diag2.passed_count, 1)

    def test_non_contiguous_frame_index_resets_streak(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        # Skip frame 1 entirely (e.g. not fed): streak must not continue on 2.
        out, diag = policy.filter((_mask(2, SQUARE),), frame_index=2)
        self.assertEqual(out, ())
        self.assertEqual(diag.suppressed_count, 1)

    def test_class_mismatch_does_not_associate(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
        )
        policy.filter((_mask(0, SQUARE, class_id=0),), frame_index=0)
        out, diag = policy.filter(
            (_mask(1, SQUARE, class_id=1, class_name="other"),),
            frame_index=1,
        )
        self.assertEqual(out, ())
        self.assertEqual(diag.suppressed_count, 1)


if __name__ == "__main__":
    unittest.main()
