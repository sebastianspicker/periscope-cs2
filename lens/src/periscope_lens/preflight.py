"""Optical preflight for camera-vs-monitor capture (ledger L4).

The three named failure modes of pointing a camera at a gaming monitor
(MIRAGE, arXiv:2602.01559, see `lens/docs/TOPOLOGY.md`): PWM flicker-banding
under rolling shutter, moiré between pixel and sensor grids, and soft focus
(autofocus hunting). This module measures all three on a captured frame and
gates the pipeline on the measurements.

Pure numpy analysis over an already-decoded grayscale frame; the *camera
configuration advice* (fixed exposure, shutter matched to refresh, focus
lock) is emitted as concrete settings strings alongside the measurements, so
the preflight ritual is "capture a test pattern → read the report → apply the
advice → re-capture", never guesswork.

Banding detection: row-mean brightness is Fourier-analyzed; a gaming monitor
under PWM + rolling shutter shows strong row-periodic oscillation, so the
ratio of band energy (high-frequency rows) to total row energy is the score.
Moiré: the same idea applied per-column for grid-beat peaks. Focus: variance
of the gradient magnitude (a numpy stand-in for variance-of-Laplacian; no
cv2 dependency in the checker itself).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

_MIN_FRAME_SIZE = 32


class PreflightError(ValueError):
    """Preflight inputs violated the contract."""


@dataclass(frozen=True)
class PreflightThresholds:
    banding_max: float = 0.20  # band-energy ratio tolerated
    moire_max: float = 0.25
    focus_min: float = 4.0  # gradient-variance floor
    coverage_margin_px: int = 8  # markers must be this far inside the frame


@dataclass(frozen=True)
class PreflightReport:
    banding_score: float
    moire_score: float
    focus_score: float
    banding_ok: bool
    moire_ok: bool
    focus_ok: bool
    coverage_ok: bool
    advice: tuple[str, ...]

    @property
    def passed(self) -> bool:
        return self.banding_ok and self.moire_ok and self.focus_ok and self.coverage_ok

    def as_dict(self) -> dict[str, object]:
        return {
            "banding_score": self.banding_score,
            "moire_score": self.moire_score,
            "focus_score": self.focus_score,
            "banding_ok": self.banding_ok,
            "moire_ok": self.moire_ok,
            "focus_ok": self.focus_ok,
            "coverage_ok": self.coverage_ok,
            "passed": self.passed,
            "advice": list(self.advice),
        }


def _validate_frame(frame: np.ndarray) -> np.ndarray:
    if frame.ndim == 3:
        frame = frame.mean(axis=2)
    if frame.ndim != 2 or min(frame.shape) < _MIN_FRAME_SIZE:
        raise PreflightError("frame must be at least 32x32 (grayscale or BGR)")
    if not np.issubdtype(frame.dtype, np.floating) and not np.issubdtype(frame.dtype, np.integer):
        raise PreflightError("frame must be numeric")
    return frame.astype(np.float64)


def banding_score(frame: np.ndarray) -> float:
    """Ratio of high-frequency row-band energy to total row energy ([0, 1]).

    A smooth panel read by a well-tuned camera yields rows that vary slowly
    (low score); PWM×rolling-shutter banding piles energy into row-periodic
    peaks (high score).
    """
    rows = _validate_frame(frame).mean(axis=1)
    centered = rows - rows.mean()
    if np.allclose(centered, 0.0):
        return 0.0
    # Score against the raw total, but measure the band on a detrended signal:
    # the linear ramp's 1/f tail otherwise swamps the high-frequency bins.
    raw = np.abs(np.fft.rfft(centered)) ** 2
    total = float(raw.sum())
    axis = np.arange(rows.size, dtype=np.float64)
    detrended = rows - np.polyval(np.polyfit(axis, rows, 1), axis)
    spectrum = np.abs(np.fft.rfft(detrended - detrended.mean())) ** 2
    if total <= 0.0:
        return 0.0
    # High frequency means row periods up to roughly 80 rows. A period of
    # four rows or less is never panel content; longer PWM bands must still
    # land above the cutoff.
    cutoff = max(3, len(spectrum) // 40)
    band = float(spectrum[cutoff:].sum())
    return min(band / total, 1.0)


def moire_score(frame: np.ndarray) -> float:
    """Ratio of grid-beat column energy to total column energy ([0, 1])."""
    columns = _validate_frame(frame).mean(axis=0)
    centered = columns - columns.mean()
    if np.allclose(centered, 0.0):
        return 0.0
    raw = np.abs(np.fft.rfft(centered)) ** 2
    total = float(raw.sum())
    axis = np.arange(columns.size, dtype=np.float64)
    detrended = columns - np.polyval(np.polyfit(axis, columns, 1), axis)
    spectrum = np.abs(np.fft.rfft(detrended - detrended.mean())) ** 2
    if total <= 0.0:
        return 0.0
    cutoff = max(3, len(spectrum) // 40)
    band = float(spectrum[cutoff:].sum())
    return min(band / total, 1.0)


def focus_score(frame: np.ndarray) -> float:
    """Variance of the gradient magnitude (numpy focus measure)."""
    image = _validate_frame(frame)
    gradient_x = np.abs(np.diff(image, axis=1))
    gradient_y = np.abs(np.diff(image, axis=0))
    magnitude = np.concatenate([gradient_x.ravel(), gradient_y.ravel()])
    return float(magnitude.var())


def coverage_check(
    marker_positions: np.ndarray,
    frame_shape: tuple[int, int],
    margin_px: int,
) -> bool:
    """True when every calibration marker is inside the frame with margin.

    ``frame_shape`` is (height, width); positions are (x, y) in pixels.
    """
    if margin_px < 0:
        raise PreflightError("margin_px must be non-negative")
    if marker_positions.size == 0:
        return False
    if marker_positions.ndim != 2 or marker_positions.shape[1] != 2:
        raise PreflightError("marker positions must be an (N, 2) array")
    height, width = frame_shape[:2]
    xs = marker_positions[:, 0]
    ys = marker_positions[:, 1]
    return bool(
        (xs >= margin_px).all()
        and (ys >= margin_px).all()
        and (xs <= width - 1 - margin_px).all()
        and (ys <= height - 1 - margin_px).all()
    )


def run_preflight(
    frame: np.ndarray,
    *,
    marker_positions: np.ndarray | None = None,
    thresholds: PreflightThresholds | None = None,
    refresh_hz: int | None = None,
) -> PreflightReport:
    """Measure, gate, and advise, one call, deterministic."""
    limits = thresholds or PreflightThresholds()
    image = _validate_frame(frame)
    banding = banding_score(image)
    moire = moire_score(image)
    focus = focus_score(image)

    coverage = True
    if marker_positions is not None:
        coverage = coverage_check(marker_positions, image.shape, limits.coverage_margin_px)

    advice: list[str] = []
    banding_ok = banding <= limits.banding_max
    moire_ok = moire <= limits.moire_max
    focus_ok = focus >= limits.focus_min
    if not banding_ok:
        advice.append(
            "banding detected: fix camera exposure (manual, shutter matched "
            f"to the panel refresh{f' ≈ 1/{refresh_hz}s' if refresh_hz else ''}), "
            "set panel brightness to 100%, disable VRR-dim modes"
        )
    if not moire_ok:
        advice.append(
            "moiré detected: increase camera distance or step down the "
            "capture resolution so the sensor grid detunes from the panel"
        )
    if not focus_ok:
        advice.append("soft focus: lock focus manually (AF hunting moves the mapping)")
    if not coverage:
        advice.append(
            "calibration markers not fully inside the frame with margin: re-frame the camera"
        )
    if not advice:
        advice.append(
            "optics clean: lock exposure/focus as configured and re-run the "
            "preflight after any camera or monitor move"
        )

    return PreflightReport(
        banding_score=banding,
        moire_score=moire,
        focus_score=focus,
        banding_ok=banding_ok,
        moire_ok=moire_ok,
        focus_ok=focus_ok,
        coverage_ok=coverage,
        advice=tuple(advice),
    )
