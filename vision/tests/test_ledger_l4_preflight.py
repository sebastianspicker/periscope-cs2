"""Ledger L4 tests: optical preflight on synthetic monitor artifacts."""

from __future__ import annotations

import numpy as np
import pytest

from cs2_vision_access.application.lens.preflight import (
    PreflightError,
    PreflightThresholds,
    banding_score,
    coverage_check,
    focus_score,
    moire_score,
    run_preflight,
)

_H, _W = 240, 320


def _clean_frame() -> np.ndarray:
    """A bright panel showing a smooth diagonal gradient (no banding, sharp
    edges, no moiré)."""
    xs = np.linspace(0, 1, _W)
    ys = np.linspace(0, 1, _H)
    return (255.0 * (np.add.outer(ys, xs) / 2.0)).astype(np.uint8)


def _banded_frame(period: int = 4, depth: float = 60.0) -> np.ndarray:
    """PWM×rolling-shutter banding: strong horizontal brightness waves."""
    base = _clean_frame().astype(np.float64)
    rows = np.arange(_H)
    band = depth * np.sin(2.0 * np.pi * rows / period)
    return np.clip(base + band[:, None], 0, 255).astype(np.uint8)


def _textured_frame(seed: int = 11) -> np.ndarray:
    """A sharp, textured panel image (natural scenes beat gradients for
    focus measures)."""
    rng = np.random.default_rng(seed)
    base = rng.uniform(60, 200, (_H // 4, _W // 4))
    upsampled = np.kron(base, np.ones((4, 4)))
    return upsampled.astype(np.uint8)


def test_banding_score_separates_clean_from_banded() -> None:
    clean = banding_score(_clean_frame())
    banded = banding_score(_banded_frame(period=4, depth=80.0))
    assert clean < 0.05
    assert banded > 0.25
    assert banded > clean


def test_moire_and_focus_measures() -> None:
    assert moire_score(_clean_frame()) < 0.2
    sharp = focus_score(_textured_frame())
    blurry = focus_score(_clean_frame())  # a smooth gradient has no texture
    assert sharp > blurry
    assert focus_score(_clean_frame()) >= 0.0


def test_preflight_gates_and_advice() -> None:
    # Banding/moiré analysis assumes smooth panel content (the preflight runs
    # on test patterns); a gradient is the canonical clean frame, and the
    # focus floor is tightened for it via thresholds.
    smooth_thresholds = PreflightThresholds(focus_min=0.1)
    report = run_preflight(_clean_frame(), refresh_hz=144, thresholds=smooth_thresholds)
    assert report.passed
    assert report.banding_ok and report.moire_ok and report.focus_ok
    assert report.coverage_ok  # no markers requested → vacuously covered
    assert any("lock exposure" in line for line in report.advice)

    banded = run_preflight(_banded_frame(depth=120.0), refresh_hz=144, thresholds=smooth_thresholds)
    assert not banded.passed
    assert not banded.banding_ok
    assert any("banding" in line for line in banded.advice)
    assert any("144" in line for line in banded.advice), "the advice names the measured refresh"


def test_coverage_gate() -> None:
    markers = np.array([[16.0, 16.0], [_W - 16.0, _H - 16.0]])
    assert coverage_check(markers, (_H, _W), margin_px=8)
    assert not coverage_check(markers, (_H, _W), margin_px=32)
    assert not coverage_check(np.empty((0, 2)), (_H, _W), margin_px=8)
    with pytest.raises(PreflightError):
        coverage_check(markers, (_H, _W), margin_px=-1)


def test_thresholds_are_tightenable() -> None:
    strict = PreflightThresholds(banding_max=0.01)
    report = run_preflight(_banded_frame(period=8, depth=30.0), thresholds=strict)
    assert not report.banding_ok
    lax = PreflightThresholds(banding_max=0.9)
    assert run_preflight(_banded_frame(period=8, depth=30.0), thresholds=lax).banding_ok


def test_frame_validation() -> None:
    with pytest.raises(PreflightError):
        run_preflight(np.zeros((8, 8), dtype=np.uint8))
    with pytest.raises(PreflightError):
        run_preflight(np.zeros((240,), dtype=np.uint8))  # 1-d input
    assert run_preflight(
        np.zeros((240, 320, 2), dtype=np.uint8),
        thresholds=PreflightThresholds(focus_min=0.0),
    ).passed  # 2-channel frames average like grayscale
    # BGR input is accepted and converted.
    bgr = np.stack([_clean_frame()] * 3, axis=2)
    assert run_preflight(bgr, thresholds=PreflightThresholds(focus_min=0.1)).passed
