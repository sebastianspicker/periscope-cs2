"""Tests for the hold-last-mask carry-forward in the temporal stability filter."""

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
OTHER = ((20.0, 20.0), (30.0, 20.0), (30.0, 30.0), (20.0, 30.0))


def _hold_policy() -> SuppressOnlyTemporalPolicy:
    return SuppressOnlyTemporalPolicy(
        TemporalStabilityConfig(
            enabled=True,
            hold_last_mask=True,
            max_dropout_frames=3,
            min_consecutive_frames=2,
        )
    )


class TemporalHoldDisabledIdentityTests(unittest.TestCase):
    def test_disabled_is_identity_even_with_hold_last_mask(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(
                enabled=False,
                hold_last_mask=True,
                max_dropout_frames=3,
            )
        )
        masks = (_mask(0, SQUARE),)
        out, diagnostics = policy.filter(masks, frame_index=0)
        self.assertEqual(out, masks)
        self.assertIs(out[0], masks[0])
        self.assertEqual(
            diagnostics,
            TemporalDiagnostics(suppressed_count=0, passed_count=1, held_count=0),
        )
        # Disabled stays identity even on a would-be dropout frame.
        out2, diagnostics2 = policy.filter((), frame_index=1)
        self.assertEqual(out2, ())
        self.assertEqual(diagnostics2.suppressed_count, 0)
        self.assertEqual(diagnostics2.passed_count, 0)
        self.assertEqual(diagnostics2.held_count, 0)


class TemporalStabilityConfigValidationTests(unittest.TestCase):
    def test_negative_max_dropout_frames_raises(self) -> None:
        with self.assertRaises(ValueError):
            TemporalStabilityConfig(max_dropout_frames=-1)

    def test_bool_max_dropout_frames_raises(self) -> None:
        with self.assertRaises(ValueError):
            TemporalStabilityConfig(max_dropout_frames=True)  # type: ignore[arg-type]

    def test_zero_min_consecutive_frames_raises(self) -> None:
        with self.assertRaises(ValueError):
            TemporalStabilityConfig(min_consecutive_frames=0)


class TemporalHoldOffBaselineTests(unittest.TestCase):
    def test_dropout_after_stable_mask_returns_nothing_when_hold_disabled(self) -> None:
        policy = SuppressOnlyTemporalPolicy(
            TemporalStabilityConfig(enabled=True, hold_last_mask=False)
        )
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        policy.filter((_mask(1, SQUARE),), frame_index=1)
        out, diagnostics = policy.filter((), frame_index=2)
        self.assertEqual(out, ())
        self.assertEqual(diagnostics.held_count, 0)
        self.assertEqual(diagnostics.passed_count, 0)
        self.assertEqual(diagnostics.suppressed_count, 0)


class TemporalHoldLastMaskCarryTests(unittest.TestCase):
    def test_held_mask_carries_for_max_dropout_frames_then_drops(self) -> None:
        policy = _hold_policy()
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        out1, diag1 = policy.filter((_mask(1, SQUARE),), frame_index=1)
        self.assertEqual(len(out1), 1)
        self.assertEqual(diag1.passed_count, 1)
        self.assertEqual(diag1.held_count, 0)

        # 1st missing frame: held.
        out2, diag2 = policy.filter((), frame_index=2)
        self.assertEqual(len(out2), 1)
        self.assertEqual(diag2.held_count, 1)
        self.assertEqual(out2[0].frame_index, 2)
        self.assertEqual(tuple(out2[0].polygon), SQUARE)

        # 2nd missing frame: still held.
        out3, diag3 = policy.filter((), frame_index=3)
        self.assertEqual(len(out3), 1)
        self.assertEqual(diag3.held_count, 1)
        self.assertEqual(out3[0].frame_index, 3)

        # 3rd missing frame: still held (frames_since_seen == 2 < 3).
        out4, diag4 = policy.filter((), frame_index=4)
        self.assertEqual(len(out4), 1)
        self.assertEqual(diag4.held_count, 1)
        self.assertEqual(out4[0].frame_index, 4)

        # 4th missing frame: frames_since_seen == 3 is NOT < max -> dropped.
        out5, diag5 = policy.filter((), frame_index=5)
        self.assertEqual(out5, ())
        self.assertEqual(diag5.held_count, 0)


class TemporalHoldUnstableNeverHeldTests(unittest.TestCase):
    def test_mask_that_never_stabilizes_is_never_held(self) -> None:
        policy = _hold_policy()
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        for frame_index in range(1, 4):
            with self.subTest(frame_index=frame_index):
                out, diagnostics = policy.filter((), frame_index=frame_index)
                self.assertEqual(out, ())
                self.assertEqual(diagnostics.held_count, 0)


class TemporalHoldGeometryFidelityTests(unittest.TestCase):
    def test_held_mask_reuses_last_stabilized_geometry(self) -> None:
        original = _mask(1, SQUARE, class_id=2, class_name="enemy", confidence=0.7)
        policy = _hold_policy()
        policy.filter(
            (_mask(0, SQUARE, class_id=2, class_name="enemy", confidence=0.7),),
            frame_index=0,
        )
        policy.filter((original,), frame_index=1)
        out, diagnostics = policy.filter((), frame_index=2)
        self.assertEqual(len(out), 1)
        self.assertEqual(diagnostics.held_count, 1)
        held = out[0]
        self.assertIsInstance(held, InstanceMask)
        self.assertEqual(tuple(held.polygon), tuple(original.polygon))
        self.assertEqual(held.frame_index, 2)
        self.assertEqual(held.class_id, 2)
        self.assertEqual(held.class_name, "enemy")
        self.assertEqual(held.confidence, 0.7)


class TemporalHoldReappearanceTests(unittest.TestCase):
    def test_reappearance_after_held_gap_passes_immediately(self) -> None:
        policy = _hold_policy()
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        policy.filter((_mask(1, SQUARE),), frame_index=1)
        policy.filter((), frame_index=2)
        policy.filter((), frame_index=3)
        out, diagnostics = policy.filter((_mask(4, SQUARE),), frame_index=4)
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0].frame_index, 4)
        self.assertEqual(diagnostics.passed_count, 1)
        self.assertEqual(diagnostics.held_count, 0)


class TemporalHoldOrderingTests(unittest.TestCase):
    def test_held_mask_is_appended_after_passed_masks(self) -> None:
        policy = _hold_policy()
        # Frame 0: only A -> not stable yet.
        policy.filter((_mask(0, SQUARE, class_id=0),), frame_index=0)
        # Frame 1: A stabilises, B appears (still unstable).
        policy.filter(
            (
                _mask(1, SQUARE, class_id=0),
                _mask(1, OTHER, class_id=1, class_name="other"),
            ),
            frame_index=1,
        )
        # Frame 2: B stabilises (passed); A is missing -> held.
        out, diagnostics = policy.filter(
            (_mask(2, OTHER, class_id=1, class_name="other"),),
            frame_index=2,
        )
        self.assertEqual(diagnostics.passed_count, 1)
        self.assertEqual(diagnostics.held_count, 1)
        self.assertEqual(len(out), 2)
        # Passed mask first (frame 2, class 1), held mask last (frame 2, class 0).
        self.assertEqual(out[0].frame_index, 2)
        self.assertEqual(out[0].class_id, 1)
        self.assertEqual(out[1].frame_index, 2)
        self.assertEqual(out[1].class_id, 0)
        self.assertEqual(tuple(out[1].polygon), SQUARE)


class TemporalDiagnosticsBackwardCompatTests(unittest.TestCase):
    def test_held_count_defaults_to_zero(self) -> None:
        diagnostics = TemporalDiagnostics(suppressed_count=0, passed_count=2)
        self.assertEqual(diagnostics.held_count, 0)

    def test_held_count_is_included_in_equality(self) -> None:
        self.assertEqual(
            TemporalDiagnostics(0, 2),
            TemporalDiagnostics(suppressed_count=0, passed_count=2, held_count=0),
        )
        self.assertNotEqual(
            TemporalDiagnostics(0, 2, held_count=1),
            TemporalDiagnostics(0, 2, held_count=0),
        )


class TemporalHoldContiguityResetTests(unittest.TestCase):
    def test_frame_index_gap_resets_state_and_prevents_holding(self) -> None:
        policy = _hold_policy()
        policy.filter((_mask(0, SQUARE),), frame_index=0)
        policy.filter((_mask(1, SQUARE),), frame_index=1)
        # Frame index jumps 2 -> 4 (non-contiguous): nothing may be held.
        out, diagnostics = policy.filter((), frame_index=4)
        self.assertEqual(out, ())
        self.assertEqual(diagnostics.held_count, 0)
        # The streak was reset, so a reappearing mask starts over (suppressed).
        out2, diagnostics2 = policy.filter((_mask(5, SQUARE),), frame_index=5)
        self.assertEqual(out2, ())
        self.assertEqual(diagnostics2.suppressed_count, 1)
        self.assertEqual(diagnostics2.held_count, 0)


if __name__ == "__main__":
    unittest.main()
