"""Screen↔camera calibration (ledger L3).

Adapted from the research ledger: arXiv:2004.08030 "Smartphone camera based
pointer" — the phone camera becomes a pointer on the *PC's* screen; the core
math is detecting the screen's corners in the camera frame and solving the
camera→screen homography. Reimplemented here (cite, don't copy; the paper is
non-exclusive-distrib).

Hardening over the paper's blind edge-detection: the preferred calibration
target is a marker set the PC displays on demand (screen corners and edge
midpoints carrying distinct codes); blind edge detection is the fallback.
A drift monitor watches live reprojection residuals so a bumped phone or an
autofocus hunt is caught instead of silently skewing every detection.

Pure numpy; no cv2 (the checker must run anywhere the package runs).
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass, field

import numpy as np

MIN_CORRESPONDENCES = 4

SequenceLikePoints = Sequence[tuple[float, float]]


class CalibrationError(ValueError):
    """Calibration inputs or solve failed the contract."""


@dataclass(frozen=True)
class Calibration:
    """A solved camera→screen mapping with a quality estimate.

    ``matrix`` is the row-major 3×3 homography normalized so ``h22 == 1``.
    """

    matrix: tuple[tuple[float, float, float], ...]
    residual_px: float
    correspondences: int

    def apply(self, camera_x: float, camera_y: float) -> tuple[float, float]:
        """Maps a camera-frame point to normalized screen coordinates."""
        (a, b, c), (d, e, f), (g, h, _i) = self.matrix
        denominator = g * camera_x + h * camera_y + _i
        if abs(denominator) < 1e-12:
            raise CalibrationError("degenerate homography denominator")
        return (
            (a * camera_x + b * camera_y + c) / denominator,
            (d * camera_x + e * camera_y + f) / denominator,
        )


@dataclass
class CalibrationMonitor:
    """Drift watchdog: tracks live residuals against the solve.

    ``drift`` becomes True when the residual exceeds ``tolerance_px`` for
    ``patience`` consecutive checks (phone bumped, autofocus hunted, monitor
    moved) — a re-solve is then required.
    """

    tolerance_px: float = 3.0
    patience: int = 5
    _residuals: list[float] = field(default_factory=list)
    _streak: int = 0

    def observe(self, residual_px: float) -> bool:
        """Records one live residual; returns True when drift is declared."""
        if residual_px < 0:
            raise CalibrationError("residual must be non-negative")
        self._residuals.append(residual_px)
        if len(self._residuals) > 64:  # bounded rolling window
            self._residuals.pop(0)
        if residual_px > self.tolerance_px:
            self._streak += 1
        else:
            self._streak = 0
        return self._streak >= self.patience

    @property
    def drift(self) -> bool:
        return self._streak >= self.patience

    @property
    def rolling_residual(self) -> float:
        return sum(self._residuals) / len(self._residuals) if self._residuals else 0.0


def screen_marker_positions(width: int, height: int) -> tuple[PointArray, ...]:
    """Screen-space marker positions for the on-demand calibration pattern:
    the four corners and the four edge midpoints of the monitor rectangle."""
    half_w = width / 2.0
    half_h = height / 2.0
    return (
        np.asarray(
            [
                [0.0, 0.0],
                [half_w, 0.0],
                [float(width), 0.0],
                [0.0, half_h],
                [float(width), half_h],
                [0.0, float(height)],
                [half_w, float(height)],
                [float(width), float(height)],
            ],
            dtype=np.float64,
        ),
    )


def _normalize(points: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Hartley normalization: centroid to origin, mean distance to sqrt(2).

    Returns (normalized homogeneous points, transform T).
    """
    centroid = points.mean(axis=0)
    shifted = points - centroid
    mean_distance = float(np.mean(np.linalg.norm(shifted, axis=1)))
    scale = float(np.sqrt(2.0)) / max(mean_distance, 1e-9)
    transform = np.array(
        [
            [scale, 0.0, -scale * centroid[0]],
            [0.0, scale, -scale * centroid[1]],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float64,
    )
    homogeneous = np.column_stack([shifted * scale, np.ones(len(shifted))])
    return homogeneous, transform


def solve_homography(
    camera_points: SequenceLikePoints,
    screen_points: SequenceLikePoints,
) -> Calibration:
    """Normalized direct linear transform (DLT) homography.

    ``camera_points``/``screen_points`` are paired (same length ≥ 4). The
    returned residual is the RMS reprojection error in screen pixels.
    """
    if len(camera_points) != len(screen_points):
        raise CalibrationError("point sets must be paired")
    if len(camera_points) < MIN_CORRESPONDENCES:
        raise CalibrationError(f"need at least {MIN_CORRESPONDENCES} correspondences")
    src = np.asarray(camera_points, dtype=np.float64)
    dst = np.asarray(screen_points, dtype=np.float64)
    if src.shape != dst.shape or src.ndim != 2 or src.shape[1] != 2:
        raise CalibrationError("points must be (N, 2) arrays of equal length")
    if not np.all(np.isfinite(src)) or not np.all(np.isfinite(dst)):
        raise CalibrationError("points must be finite")

    src_h, t_src = _normalize(src)
    dst_h, t_dst = _normalize(dst)

    rows: list[np.ndarray] = []
    for (x, y, w), (u, v, _) in zip(src_h, dst_h, strict=True):
        rows.append(np.array([x, y, w, 0.0, 0.0, 0.0, -u * x, -u * y, -u * w]))
        rows.append(np.array([0.0, 0.0, 0.0, x, y, w, -v * x, -v * y, -v * w]))
    system = np.asarray(rows)
    _, _, vh = np.linalg.svd(system)
    solution = vh[-1]

    h_normalized = solution.reshape(3, 3)
    # Denormalize: H = inv(T_dst) @ h_normalized @ T_src
    matrix = np.linalg.inv(t_dst) @ h_normalized @ t_src
    if abs(matrix[2, 2]) < 1e-12:
        raise CalibrationError("degenerate homography (h22 ~ 0)")
    matrix = matrix / matrix[2, 2]

    residual = _reprojection_residual(matrix, src, dst)
    return Calibration(
        matrix=tuple((float(row[0]), float(row[1]), float(row[2])) for row in matrix),
        residual_px=residual,
        correspondences=len(src),
    )


def residual_for(
    calibration: Calibration,
    camera_points: SequenceLikePoints,
    screen_points: SequenceLikePoints,
) -> float:
    """RMS reprojection residual of fresh correspondences against a solve —
    the number the drift monitor consumes."""
    src = np.asarray(camera_points, dtype=np.float64)
    dst = np.asarray(screen_points, dtype=np.float64)
    if src.shape != dst.shape or src.size == 0:
        raise CalibrationError("paired, non-empty point sets required")
    return _reprojection_residual(np.asarray(calibration.matrix, dtype=np.float64), src, dst)


def _reprojection_residual(matrix: np.ndarray, src: np.ndarray, dst: np.ndarray) -> float:
    homogeneous = np.column_stack([src, np.ones(len(src))])
    projected = homogeneous @ matrix.T
    denominator = projected[:, 2]
    if np.any(np.abs(denominator) < 1e-12):
        raise CalibrationError("degenerate homography denominator in residual")
    projected = projected[:, :2] / denominator[:, None]
    errors = np.linalg.norm(projected - dst, axis=1)
    return float(np.sqrt(np.mean(errors**2)))


PointArray = np.ndarray
