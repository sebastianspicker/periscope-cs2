"""Ledger V3 tests: device profiles and bounded latency telemetry."""

from __future__ import annotations

import pytest

from cs2_vision_access.application.live.performance import (
    FALLBACK_PROFILE,
    LatencyWindow,
    classify_renderer,
    describe_active,
    select_profile,
)


def test_classification_table() -> None:
    assert classify_renderer("Apple M2 Pro").name == "apple-silicon"
    assert classify_renderer("NVIDIA GeForce RTX 4070").name == "nvidia-dedicated"
    assert classify_renderer("AMD Radeon RX 7800 XT").name == "amd-dedicated"
    assert classify_renderer("Intel(R) Iris(R) Xe Graphics").name == ("intel-integrated")
    assert classify_renderer("llvmpipe (LLVM 17)").name == "software-renderer"
    # First match wins when several keywords appear.
    assert classify_renderer("NVIDIA vs AMD test string").name == "nvidia-dedicated"


def test_unknown_renderer_falls_back_conservatively() -> None:
    profile = classify_renderer("Mystery Graphics 9000")
    assert profile == FALLBACK_PROFILE
    assert profile.state_reader_hz == 30.0  # conservative, not optimistic


def test_explicit_overrides_win_and_unknown_keys_rejected() -> None:
    profile = select_profile("NVIDIA RTX", {"state_reader_hz": 120.0})
    assert profile.state_reader_hz == 120.0
    assert profile.frame_skip == 2  # untouched fields stay from the profile

    with pytest.raises(ValueError, match="unknown profile overrides"):
        select_profile("NVIDIA RTX", {"state_readre_hz": 120.0})


def test_description_always_names_the_actual_renderer() -> None:
    text = describe_active("Mystery GPU", classify_renderer("Mystery GPU"))
    assert "Mystery GPU" in text
    assert "advisory" in text


def test_latency_window_statistics_and_bounds() -> None:
    window = LatencyWindow(maxlen=5)
    assert window.summary() == (0.0, 0.0)  # empty window never lies
    for capture, present in [
        (1_000_000_000, 1_040_000_000),  # 40 ms
        (2_000_000_000, 2_060_000_000),  # 60 ms
        (3_000_000_000, 3_050_000_000),  # 50 ms
        (4_000_000_000, 4_045_000_000),  # 45 ms
        (5_000_000_000, 5_055_000_000),  # 55 ms
        (6_000_000_000, 6_070_000_000),  # 70 ms, evicts the 40 ms sample
    ]:
        assert window.record(capture, present)
    assert window.count == 5
    p50, p95 = window.summary()
    assert 50.0 <= p50 <= 60.0
    assert p95 >= p50

    # Clock glitches are rejected, not recorded.
    assert not window.record(9_000_000_000, 8_000_000_000)
    assert window.count == 5


def test_latency_window_fraction_validation() -> None:
    window = LatencyWindow()
    window.record(0, 1_000_000)
    with pytest.raises(ValueError):
        window.percentile(1.5)
