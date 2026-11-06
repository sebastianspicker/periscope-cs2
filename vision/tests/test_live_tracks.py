"""Tests for the ledger E6/E7/E8/E9/E31 live-pipeline additions.

Covers: the shared association primitives, the track smoother/animator
(persistent ids, exponential smoothing, fade-in/out, pruning, config
validation), the prediction plausibility gate, the scene-change detector,
the temporal policy reset, and the rendered-frame immutability contract
across the shared-memory sink.
"""

from __future__ import annotations

import numpy as np
import pytest

from cs2_vision_access.application.live.inference.association import (
    associate_greedy,
    bbox_iou,
)
from cs2_vision_access.application.live.inference.association import (
    bbox as polygon_bbox,
)
from cs2_vision_access.application.live.inference.association import (
    centroid as polygon_centroid,
)
from cs2_vision_access.application.live.inference.scene import (
    SceneChangeConfig,
    SceneChangeDetector,
)
from cs2_vision_access.application.live.inference.temporal import (
    SuppressOnlyTemporalPolicy,
    TemporalStabilityConfig,
)
from cs2_vision_access.application.live.inference.tracks import (
    TrackAnimator,
    TrackSmoothingConfig,
)
from cs2_vision_access.application.live.quality import (
    GateDiagnostics,
    PredictionGateConfig,
    gate_predictions,
)
from cs2_vision_access.domain.predictions import InstanceMask


def _mask(
    x0: float,
    y0: float,
    x1: float,
    y1: float,
    *,
    frame_index: int = 0,
    class_id: int = 0,
    class_name: str = "player",
    confidence: float = 0.9,
) -> InstanceMask:
    polygon = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
    return InstanceMask(
        frame_index=frame_index,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=class_name,
    )


# ---------------------------------------------------------------- association


def test_association_bbox_iou_and_centroid() -> None:
    assert bbox_iou((0.0, 0.0, 10.0, 10.0), (0.0, 0.0, 10.0, 10.0)) == pytest.approx(1.0)
    assert bbox_iou((0.0, 0.0, 10.0, 10.0), (20.0, 20.0, 30.0, 30.0)) == 0.0
    assert polygon_centroid(((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0))) == (5.0, 5.0)
    assert polygon_bbox(((1.0, 2.0), (3.0, 4.0))) == (1.0, 2.0, 3.0, 4.0)


def test_associate_greedy_prefers_higher_iou() -> None:
    previous = [(0, (5.0, 5.0), (0.0, 0.0, 10.0, 10.0))]
    # Two currents overlap the previous; the closer/higher-IoU one wins.
    current = [
        (0, (5.0, 5.0), (1.0, 1.0, 11.0, 11.0)),
        (0, (5.0, 5.0), (2.0, 2.0, 12.0, 12.0)),
    ]
    mapping = associate_greedy(previous, current, iou_threshold=0.1, centroid_match_px=64.0)
    # Only one previous slot: the higher-IoU candidate (index 0) takes it.
    assert mapping == {0: 0}


# ------------------------------------------------------------ track animator


def test_animator_disabled_is_identity() -> None:
    animator = TrackAnimator(TrackSmoothingConfig(enabled=False))
    masks = (_mask(0, 0, 10, 10, frame_index=3),)
    emitted, diag = animator.update(masks, frame_index=3)
    assert emitted == masks
    assert diag.smoothed_count == 0
    assert diag.active_tracks == 0


def test_animator_smooths_jitter_toward_detection() -> None:
    animator = TrackAnimator(TrackSmoothingConfig(enabled=True, smoothing_rate=12.0))
    first, _ = animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    assert len(first) == 1
    # A small detection move must be damped, not followed exactly.
    second, diag = animator.update((_mask(10, 0, 110, 100, frame_index=1),), frame_index=1)
    assert diag.smoothed_count == 1
    box = polygon_bbox(second[0].polygon)
    assert 0.0 < box[0] < 10.0  # moved partway toward the target
    assert diag.active_tracks == 1


def test_animator_uses_exp_frame_rate_independent_integration() -> None:
    # One update with rate k must move the box by exactly 1-exp(-k).
    animator = TrackAnimator(TrackSmoothingConfig(enabled=True, smoothing_rate=6.0))
    animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    emitted, _ = animator.update((_mask(10, 0, 110, 100, frame_index=1),), frame_index=1)
    expected_shift = 10.0 * (1.0 - np.exp(-6.0))
    box = polygon_bbox(emitted[0].polygon)
    assert box[0] == pytest.approx(expected_shift, rel=1e-6)


def test_animator_fade_in_grows_then_full() -> None:
    animator = TrackAnimator(TrackSmoothingConfig(enabled=True, fade_in_frames=3))
    first, _ = animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    grown_box = polygon_bbox(first[0].polygon)
    assert grown_box[2] - grown_box[0] < 100.0  # scaled in, not full yet
    for index in (1, 2, 3):
        emitted, _ = animator.update((_mask(0, 0, 100, 100, frame_index=index),), frame_index=index)
    final_box = polygon_bbox(emitted[0].polygon)
    assert final_box[2] - final_box[0] == pytest.approx(100.0)


def test_animator_fade_out_shrinks_then_drops() -> None:
    animator = TrackAnimator(
        TrackSmoothingConfig(enabled=True, fade_in_frames=0, fade_out_frames=2, max_gap_frames=4)
    )
    animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    fading, diag = animator.update((), frame_index=1)
    assert diag.fading_count == 1
    faded_box = polygon_bbox(fading[0].polygon)
    assert 0.0 < faded_box[2] - faded_box[0] < 100.0
    # After the fade window and gap are exhausted the track is gone.
    animator.update((), frame_index=2)
    animator.update((), frame_index=3)
    animator.update((), frame_index=4)
    animator.update((), frame_index=5)
    _last, diag_last = animator.update((), frame_index=6)
    assert diag_last.fading_count == 0
    assert diag_last.active_tracks == 0


def test_animator_fading_confidence_decays_and_stays_valid() -> None:
    animator = TrackAnimator(
        TrackSmoothingConfig(enabled=True, fade_in_frames=0, fade_out_frames=3, max_gap_frames=5)
    )
    animator.update((_mask(0, 0, 100, 100, confidence=0.8, frame_index=0),), frame_index=0)
    fading, _ = animator.update((), frame_index=1)
    assert 0.0 <= fading[0].confidence < 0.8


def test_animator_class_change_starts_new_track_old_fades() -> None:
    """Association is same-class-only (matching the temporal filter), so a
    class flip legitimately retires the old track (fading) and starts a new
    one; the flip itself is the cold-field invalidation (E9)."""
    animator = TrackAnimator(
        TrackSmoothingConfig(enabled=True, fade_in_frames=0, fade_out_frames=1, max_gap_frames=4)
    )
    animator.update(
        (_mask(0, 0, 100, 100, class_id=1, class_name="a", frame_index=0),), frame_index=0
    )
    emitted, diag = animator.update(
        (_mask(0, 0, 100, 100, class_id=2, class_name="b", frame_index=1),), frame_index=1
    )
    # New-class detection emitted live; the old-class track emits a fading copy.
    assert {m.class_name for m in emitted} == {"a", "b"}
    assert diag.active_tracks == 2
    # The old track's fade window (1 frame) exhausts; the new track persists
    # until its identity memory (max_gap_frames=4) is exhausted. One update
    # per inferred frame — the animator counts updates, not index gaps.
    for frame in range(2, 8):
        _later, diag_later = animator.update((), frame_index=frame)
    assert diag_later.fading_count == 0
    assert diag_later.active_tracks == 0


def test_animator_persists_track_across_gap_and_rematches() -> None:
    animator = TrackAnimator(
        TrackSmoothingConfig(enabled=True, fade_in_frames=0, fade_out_frames=2, max_gap_frames=3)
    )
    animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    _fading, diag = animator.update((), frame_index=1)
    assert diag.fading_count == 1
    assert diag.active_tracks == 1
    rematched, diag2 = animator.update((_mask(0, 0, 100, 100, frame_index=2),), frame_index=2)
    # Same physical object: one emission, no fading copy, one active track.
    assert len(rematched) == 1
    assert diag2.smoothed_count == 1
    assert diag2.fading_count == 0
    assert diag2.active_tracks == 1


def test_animator_prunes_beyond_max_tracks() -> None:
    animator = TrackAnimator(
        TrackSmoothingConfig(enabled=True, max_tracks=2, fade_in_frames=0, max_gap_frames=0)
    )
    # Three distinct far-apart detections: the oldest track must be pruned.
    for frame, x in ((0, 0.0), (1, 500.0), (2, 1000.0)):
        _out, diag = animator.update(
            (_mask(x, 0, x + 50, 50, frame_index=frame),), frame_index=frame
        )
    assert diag.active_tracks <= 2


def test_animator_reset_clears_tracks() -> None:
    animator = TrackAnimator(TrackSmoothingConfig(enabled=True, fade_in_frames=0))
    animator.update((_mask(0, 0, 100, 100, frame_index=0),), frame_index=0)
    animator.reset()
    _out, diag = animator.update((), frame_index=1)
    assert diag.active_tracks == 0
    assert diag.fading_count == 0


def test_animator_config_validation() -> None:
    with pytest.raises(ValueError):
        TrackSmoothingConfig(smoothing_rate=-1.0)
    with pytest.raises(ValueError):
        TrackSmoothingConfig(fade_in_frames=-1)
    with pytest.raises(ValueError):
        TrackSmoothingConfig(max_tracks=0)
    with pytest.raises(ValueError):
        TrackAnimator(iou_threshold=1.5)
    with pytest.raises(ValueError):
        TrackAnimator(centroid_match_px=0.0)


# ------------------------------------------------------------ quality gate


def test_gate_disabled_is_identity() -> None:
    masks = (_mask(0, 0, 10, 10, confidence=0.05),)
    passed, diag = gate_predictions(
        masks, PredictionGateConfig(enabled=False), frame_width=100, frame_height=100
    )
    assert passed == masks
    assert diag == GateDiagnostics(passed_count=1)


def test_gate_drops_low_confidence_and_small_area() -> None:
    config = PredictionGateConfig(enabled=True, min_confidence=0.5, min_area_px=100.0)
    good = _mask(0, 0, 50, 50, confidence=0.8)
    weak = _mask(10, 10, 20, 20, confidence=0.2)
    tiny = _mask(0, 0, 5, 5, confidence=0.9)
    passed, diag = gate_predictions((weak, good, tiny), config, frame_width=200, frame_height=200)
    assert passed == (good,)
    assert diag.dropped_low_confidence == 1
    assert diag.dropped_small_area == 1
    assert diag.dropped_count == 2


def test_gate_drops_full_frame_detections() -> None:
    config = PredictionGateConfig(enabled=True, max_area_fraction=0.5)
    huge = _mask(0, 0, 200, 200, confidence=0.9)
    passed, diag = gate_predictions((huge,), config, frame_width=200, frame_height=200)
    assert passed == ()
    assert diag.dropped_large_area == 1


def test_gate_class_allowlists() -> None:
    config = PredictionGateConfig(enabled=True, allowed_class_ids=(1,))
    by_id = _mask(0, 0, 10, 10, class_id=1, class_name="enemy")
    by_name = _mask(0, 0, 10, 10, class_id=2, class_name="other")
    passed, diag = gate_predictions((by_id, by_name), config, frame_width=100, frame_height=100)
    assert passed == (by_id,)
    assert diag.dropped_class_filter == 1

    name_config = PredictionGateConfig(enabled=True, allowed_class_names=("other",))
    passed2, diag2 = gate_predictions(
        (by_id, by_name), name_config, frame_width=100, frame_height=100
    )
    assert passed2 == (by_name,)
    assert diag2.dropped_class_filter == 1


def test_gate_drops_nonfinite_geometry() -> None:
    config = PredictionGateConfig(enabled=True)
    broken = InstanceMask(
        frame_index=0,
        polygon=((0.0, 0.0), (float("nan"), 1.0), (2.0, 2.0)),
        confidence=0.9,
        class_id=0,
        class_name="player",
    )
    passed, diag = gate_predictions((broken,), config, frame_width=100, frame_height=100)
    assert passed == ()
    assert diag.dropped_nonfinite == 1


def test_gate_config_validation() -> None:
    with pytest.raises(ValueError):
        PredictionGateConfig(min_confidence=1.5)
    with pytest.raises(ValueError):
        PredictionGateConfig(max_area_fraction=0.0)
    with pytest.raises(ValueError):
        PredictionGateConfig(min_area_px=-1.0)
    with pytest.raises(ValueError):
        PredictionGateConfig(allowed_class_ids=[1, 2])  # list, not tuple


# ------------------------------------------------------- scene-change detector


def test_scene_detector_disabled_never_fires() -> None:
    detector = SceneChangeDetector(SceneChangeConfig(enabled=False))
    frame = np.zeros((32, 32, 3), dtype=np.uint8)
    assert detector.update(frame) is False
    assert detector.update(np.full((32, 32, 3), 255, dtype=np.uint8)) is False


def test_scene_detector_first_frame_is_not_a_cut() -> None:
    detector = SceneChangeDetector(SceneChangeConfig(enabled=True, threshold=10.0))
    frame = np.zeros((32, 32, 3), dtype=np.uint8)
    assert detector.update(frame) is False


def test_scene_detector_fires_on_cut_and_respects_cooldown() -> None:
    detector = SceneChangeDetector(
        SceneChangeConfig(enabled=True, threshold=10.0, cooldown_frames=2)
    )
    dark = np.zeros((32, 32, 3), dtype=np.uint8)
    bright = np.full((32, 32, 3), 255, dtype=np.uint8)
    assert detector.update(dark) is False  # initialize
    assert detector.update(bright) is True  # cut
    # Cooldown: even a big flip right after is debounced.
    assert detector.update(dark) is False
    assert detector.update(bright) is False
    assert detector.update(dark) is True


def test_scene_detector_quiet_frames_do_not_fire() -> None:
    detector = SceneChangeDetector(SceneChangeConfig(enabled=True, threshold=10.0))
    frame = np.full((32, 32, 3), 40, dtype=np.uint8)
    assert detector.update(frame) is False
    frame_noisy = frame.copy()
    frame_noisy[0, 0, 0] = 45  # tiny change below threshold
    assert detector.update(frame_noisy) is False


def test_scene_detector_resolution_change_is_a_cut() -> None:
    detector = SceneChangeDetector(SceneChangeConfig(enabled=True, threshold=250.0))
    assert detector.update(np.zeros((32, 32, 3), dtype=np.uint8)) is False
    # A different thumbnail shape reports a cut regardless of threshold.
    assert detector.update(np.zeros((64, 64, 3), dtype=np.uint8)) is True


def test_scene_detector_reset() -> None:
    detector = SceneChangeDetector(SceneChangeConfig(enabled=True, threshold=10.0))
    detector.update(np.zeros((32, 32, 3), dtype=np.uint8))
    detector.reset()
    # After reset there is no previous frame, so no cut can fire.
    assert detector.update(np.full((32, 32, 3), 255, dtype=np.uint8)) is False


# ------------------------------------------------------- temporal reset + E8


def test_temporal_policy_reset_restarts_stability() -> None:
    policy = SuppressOnlyTemporalPolicy(
        TemporalStabilityConfig(enabled=True, min_consecutive_frames=2)
    )
    mask = _mask(0, 0, 10, 10, frame_index=0)
    _first, _ = policy.filter((mask,), frame_index=0, sequence=0)
    policy.reset()
    _second, diag = policy.filter((mask,), frame_index=1, sequence=1)
    # History was cleared: the detection must count as first-seen again.
    assert diag.passed_count == 0
    assert diag.suppressed_count == 1


def test_shared_sink_accepts_read_only_frame() -> None:
    """E8 contract: the SHM sink consumes the read-only rendered snapshot."""
    from cs2_vision_access.adapters.capture.sinks.shared import (
        SharedFrameSink,
        read_shared_frame,
        read_shared_header,
    )

    frame = np.zeros((4, 6, 4), dtype=np.uint8)
    frame[..., 0] = 7
    frame.setflags(write=False)
    with SharedFrameSink("e8_readonly_test", 6, 4, slots=2) as sink:
        sink.send(frame, 0)
        header = read_shared_header(sink._shm)
        assert header.state == 1
        out = read_shared_frame(sink._shm, 6, 4, 2)
    assert out.shape == (4, 6, 4)
    assert int(out[0, 0, 0]) == 7
