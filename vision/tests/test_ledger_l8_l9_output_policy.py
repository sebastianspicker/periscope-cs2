"""Ledger L8 + L9 tests: output-path ladder, HID shaping, assist policy.

Determinism is the acceptance rule: the same input sequence produces the
same motion sequence, and no default-on actuation exists anywhere (the
policy only computes deltas; nothing here sends input).
"""

from __future__ import annotations

import pytest

from cs2_vision_access.application.lens.output import (
    HID_AXIS_MAX,
    HID_AXIS_MIN,
    HidDeltaShaper,
    OutputFeatures,
    OutputPath,
    encode_hid_report,
    select_output_path,
)
from cs2_vision_access.application.lens.policy import (
    AssistPolicy,
    PolicyError,
    PolicyState,
    motion_delta,
    select_target,
)
from cs2_vision_access.application.lens.transport import TargetBox

# ── L8: output paths ──────────────────────────────────────────────


def test_output_path_ladder() -> None:
    assert select_output_path(OutputFeatures(True, True)) is OutputPath.HID_GADGET
    assert select_output_path(OutputFeatures(False, True)) is OutputPath.PC_AGENT
    assert select_output_path(OutputFeatures(False, False)) is (OutputPath.TARGETS_ONLY), (
        "targets-only is the always-available degradation"
    )


def test_hid_delta_shaper_accumulates_and_clamps() -> None:
    shaper = HidDeltaShaper(max_counts_per_report=40)
    shaper.add(300.0, -50.0)
    reports = list(shaper.reports())
    assert sum(dx for dx, _ in reports) == 300
    assert sum(dy for _, dy in reports) == -50
    assert all(abs(dx) <= 40 and abs(dy) <= 40 for dx, dy in reports)
    assert shaper.pending == (0.0, 0.0), "drained"


def test_hid_delta_shaper_rate_limits_large_corrections() -> None:
    shaper = HidDeltaShaper(max_counts_per_report=5)
    shaper.add(12.0, 0.0)
    reports = list(shaper.reports())
    assert len(reports) >= 3, "a violent correction lands as several steps"
    assert all(dx <= 5 for dx, _ in reports)


def test_hid_shaper_rejects_nan_and_recovers() -> None:
    shaper = HidDeltaShaper()
    with pytest.raises(ValueError, match="finite"):
        shaper.add(float("nan"), 1.0)
    with pytest.raises(ValueError, match="finite"):
        shaper.add(1.0, float("inf"))
    # A non-finite value forced into the accumulator cannot wedge reports().
    shaper.add(0.0, 0.0)
    shaper._acc_x = float("nan")  # type: ignore[assignment]
    assert list(shaper.reports()) == []
    assert shaper.pending == (0.0, 0.0)


def test_hid_report_encoding_range_checks() -> None:
    assert encode_hid_report(0, -1, 1) == bytes((1, 0, 0xFF))
    assert encode_hid_report(127, -127) == bytes((0, 127, 0x81))
    with pytest.raises(ValueError, match="dx out of HID range"):
        encode_hid_report(128, 0)
    with pytest.raises(ValueError, match="dy out of HID range"):
        encode_hid_report(0, -128)
    assert HID_AXIS_MIN == -127 and HID_AXIS_MAX == 127


def test_hid_shaper_subcount_residuals_wait() -> None:
    shaper = HidDeltaShaper()
    shaper.add(0.4, 0.6)  # below one count on each axis
    assert list(shaper.reports()) == []
    shaper.add(0.4, 0.6)  # now 0.8/1.2 → y emits 1, x stays pending
    assert list(shaper.reports()) == [(0, 1)]
    px, py = shaper.pending
    assert px == pytest.approx(0.8, abs=1e-9)
    assert py == pytest.approx(0.2, abs=1e-9)


# ── L9: policy ────────────────────────────────────────────────────


def _box(cx: float, cy: float) -> TargetBox:
    return TargetBox(class_id=0, confidence=0.9, cx=cx, cy=cy, width=20.0, height=40.0)


def test_fov_gate_excludes_distant_targets() -> None:
    policy = AssistPolicy(fov_px=100.0)
    state = PolicyState()
    assert select_target([_box(500.0, 500.0)], (0.0, 0.0), policy, state) is None
    picked = select_target([_box(30.0, 40.0)], (0.0, 0.0), policy, state)
    assert picked is not None and picked[0] == 0
    assert state.locked_target_id == 0


def test_sticky_lock_and_switch_margin() -> None:
    policy = AssistPolicy(fov_px=500.0, sticky_switch_px=10.0)
    state = PolicyState()
    first = select_target([_box(100.0, 0.0), _box(90.0, 0.0)], (0.0, 0.0), policy, state)
    assert first is not None and first[0] == 1  # closest wins
    # A marginally closer target does not steal the lock.
    state2 = PolicyState()
    base = [_box(100.0, 0.0), _box(90.0, 0.0)]
    locked = select_target(base, (0.0, 0.0), policy, state2, target_ids=[7, 8])
    assert locked is not None and locked[0] == 8
    sticky = select_target(base, (0.0, 0.0), policy, state2, target_ids=[7, 8])
    assert sticky is not None and sticky[0] == 8, "stickiness holds"
    decisive = select_target(
        [_box(70.0, 0.0), _box(90.0, 0.0)],
        (0.0, 0.0),
        policy,
        state2,
        target_ids=[7, 8],
    )
    assert decisive is not None and decisive[0] == 7, "out-sticked decisively"


def test_motion_delta_smoothing_deadzone_and_cap() -> None:
    policy = AssistPolicy(
        smoothing=0.5, max_step_px=10.0, dead_zone_px=2.0, large_step_px=1000.0
    )  # never triggers cooldown
    state = PolicyState()
    # Dead zone: tiny residuals are ignored.
    assert motion_delta((101.0, 100.0), (100.0, 100.0), policy, state) == (0.0, 0.0)
    # Cap: a 100 px error moves at most 10 px per frame (×(1−smoothing)).
    dx, dy = motion_delta((200.0, 100.0), (100.0, 100.0), policy, state)
    assert abs(dx) == pytest.approx(10.0 * 0.5)
    assert dy == 0.0


def test_motion_delta_cooldown_after_large_step() -> None:
    policy = AssistPolicy(
        smoothing=0.0, max_step_px=60.0, large_step_px=50.0, cooldown_frames_after_large_step=2
    )
    state = PolicyState()
    dx, _ = motion_delta((160.0, 100.0), (100.0, 100.0), policy, state)
    assert dx == pytest.approx(60.0)  # capped large step
    assert state.cooldown == 2
    assert motion_delta((160.0, 100.0), (100.0, 100.0), policy, state) == (0.0, 0.0)
    assert motion_delta((160.0, 100.0), (100.0, 100.0), policy, state) == (0.0, 0.0)
    # Cooldown expired: motion resumes.
    dx2, _ = motion_delta((160.0, 100.0), (100.0, 100.0), policy, state)
    assert dx2 == pytest.approx(60.0)


def test_policy_validation_and_determinism() -> None:
    with pytest.raises(PolicyError):
        AssistPolicy(fov_px=0.0)
    with pytest.raises(PolicyError):
        AssistPolicy(smoothing=1.5)
    # Determinism: same inputs → same motion sequence.
    policy = AssistPolicy()
    outputs = []
    for _ in range(2):
        state = PolicyState()
        sequence = []
        for _index in range(5):
            picked = select_target([_box(60.0, 0.0)], (0.0, 0.0), policy, state)
            assert picked is not None
            sequence.append(motion_delta((60.0, 0.0), (0.0, 0.0), policy, state))
        outputs.append(sequence)
    assert outputs[0] == outputs[1]
