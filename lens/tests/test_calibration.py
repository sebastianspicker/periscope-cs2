"""Ledger L3 tests: screen↔camera homography calibration."""

from __future__ import annotations

import numpy as np
import pytest

from periscope_lens.calibration import (
    CalibrationError,
    CalibrationMonitor,
    residual_for,
    screen_marker_positions,
    solve_homography,
)


def _affine_camera_to_screen(
    width: int = 1920, height: int = 1080, scale: float = 0.4, offset: float = 120.0
):
    """A synthetic camera view of the screen: scale + offset + slight skew."""
    matrix = np.array(
        [
            [scale, 0.01 * scale, offset],
            [0.0, scale, offset * 0.5],
            [0.0, 0.0, 1.0],
        ]
    )
    markers = screen_marker_positions(width, height)
    homogeneous = np.column_stack([markers, np.ones(len(markers))])
    projected = homogeneous @ matrix.T
    camera = projected[:, :2] / projected[:, 2:3]
    return matrix, markers, camera


def test_exact_solve_recovers_mapping() -> None:
    _matrix, markers, camera = _affine_camera_to_screen()
    calibration = solve_homography(
        [tuple(point) for point in camera], [tuple(point) for point in markers]
    )
    assert calibration.residual_px < 1e-6
    assert calibration.correspondences == 8
    for camera_point, screen_point in zip(camera, markers, strict=True):
        mapped = calibration.apply(float(camera_point[0]), float(camera_point[1]))
        assert mapped == pytest.approx((screen_point[0], screen_point[1]), abs=1e-6)


def test_perspective_projection_is_recoverable() -> None:
    """A true projective warp (keystone), the case plain affine cannot fit."""
    width, height = 1280, 720
    markers = screen_marker_positions(width, height)
    projective = np.array(
        [
            [0.35, 0.01, 150.0],
            [0.0, 0.35, 90.0],
            [2e-5, 1e-5, 1.0],
        ]
    )
    homogeneous = np.column_stack([markers, np.ones(len(markers))])
    projected = homogeneous @ projective.T
    camera = projected[:, :2] / projected[:, 2:3]
    calibration = solve_homography(
        [tuple(point) for point in camera], [tuple(point) for point in markers]
    )
    assert calibration.residual_px < 1e-4
    for camera_point, screen_point in zip(camera, markers, strict=True):
        mapped = calibration.apply(float(camera_point[0]), float(camera_point[1]))
        assert mapped == pytest.approx((screen_point[0], screen_point[1]), abs=1e-4)


def test_noise_degrades_residual_gracefully() -> None:
    _matrix, markers, camera = _affine_camera_to_screen()
    noisy = camera + np.random.default_rng(7).normal(0.0, 1.0, camera.shape)
    calibration = solve_homography(
        [tuple(point) for point in noisy], [tuple(point) for point in markers]
    )
    # A one-pixel jitter must land in a one-pixel-ish residual, not blow up.
    assert 0.0 < calibration.residual_px < 5.0


def test_residual_for_detects_drift() -> None:
    _matrix, markers, camera = _affine_camera_to_screen()
    calibration = solve_homography(
        [tuple(point) for point in camera], [tuple(point) for point in markers]
    )
    bumped = camera + np.array([15.0, 0.0])  # phone moved sideways
    residual = residual_for(calibration, [tuple(p) for p in bumped], [tuple(p) for p in markers])
    assert residual > 10.0


def test_input_validation() -> None:
    with pytest.raises(CalibrationError, match="paired"):
        solve_homography([(0, 0), (1, 1), (2, 2), (3, 3)], [(0, 0), (1, 1)])
    with pytest.raises(CalibrationError, match="at least 4"):
        solve_homography([(0, 0), (1, 1), (2, 2)], [(0, 0), (1, 1), (2, 2)])
    with pytest.raises(CalibrationError, match="finite"):
        solve_homography(
            [(0, 0), (1, 1), (2, 2), (float("nan"), 3)],
            [(0, 0), (1, 1), (2, 2), (3, 3)],
        )
    # Collinear points are a legal 1-D projective map.
    collinear = solve_homography(
        [(0, 0), (1, 0), (2, 0), (3, 0)],
        [(0, 0), (1, 0), (2, 0), (3, 0)],
    )
    assert collinear.apply(2.0, 0.0) == pytest.approx((2.0, 0.0), abs=1e-6)


def test_drift_monitor_patience() -> None:
    monitor = CalibrationMonitor(tolerance_px=3.0, patience=3)
    assert not monitor.observe(1.0)  # clean check: streak reset
    assert not monitor.observe(10.0)  # breach 1
    assert not monitor.observe(10.0)  # breach 2
    assert not monitor.drift  # streak 2 < patience 3
    assert monitor.observe(10.0)  # breach 3 → drift declared
    assert monitor.drift
    assert not monitor.observe(1.0)  # good check resets the streak
    assert not monitor.drift
    assert monitor.rolling_residual > 0.0
    with pytest.raises(CalibrationError):
        monitor.observe(-1.0)


def test_marker_positions_are_one_array() -> None:
    markers = screen_marker_positions(1920, 1080)
    assert isinstance(markers, np.ndarray)
    assert markers.shape == (8, 2)


def test_residual_for_rejects_malformed_points() -> None:
    calibration = solve_homography(
        [(0, 0), (1, 0), (0, 1), (1, 1)],
        [(0, 0), (10, 0), (0, 10), (10, 10)],
    )
    with pytest.raises(CalibrationError, match=r"\(N, 2\)"):
        residual_for(calibration, [(0, 0, 0), (1, 1, 1)], [(0, 0), (1, 1)])
    with pytest.raises(CalibrationError, match="finite"):
        residual_for(calibration, [(0, 0), (float("inf"), 1)], [(0, 0), (1, 1)])
