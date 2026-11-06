"""Latency measurement and compensation (ledger L7).

The rule from the ledger: **latency is measured, never assumed.** Every
compensation value carries its provenance (measured / assumed) so the debug
surface can show the difference — the ReaSnow forums are full of "feels
laggy" because that distinction was never drawn.

Components:

- :class:`RoundTrip` — ping/pong RTT tracker over the L6 transport, bounded,
  with a cap on accepted samples and outlier rejection (a single scheduler
  hiccup must not inflate the estimate).
- :class:`VelocityPredictor` — per-target constant-velocity (alpha-beta)
  predictor that extrapolates a target by the *measured* end-to-end latency.
- :class:`LatencyBreakdown` — the honest record: which segments were
  measured (camera→detect from camera timestamps vs detect timestamps,
  transport from RTT/2) and which were assumed.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

_MAX_SAMPLES = 256
_OUTLIER_FACTOR = 4.0


class LatencyError(ValueError):
    """Latency inputs violated the contract."""


@dataclass
class RoundTrip:
    """Bounded RTT tracker with single-sample outlier rejection.

    A *sustained* regime change (transport switched from adb-forward to
    RNDIS, USB contention appeared) must not be rejected forever by a stale
    median, so two consecutive outliers are adopted as the new regime and
    the old window is dropped.
    """

    window: int = _MAX_SAMPLES
    _samples_ms: deque[float] = field(default_factory=lambda: deque())
    _consecutive_rejects: int = 0

    def __post_init__(self) -> None:
        if self.window < 1:
            raise LatencyError("window must be positive")
        self._samples_ms = deque(self._samples_ms, maxlen=self.window)

    def record(self, t0_ns: int, reply_ns: int) -> bool:
        """Records one completed ping/pong exchange; returns False when the
        sample is rejected (negative, or an outlier vs the running median ×
        factor)."""
        if reply_ns < t0_ns:
            return False
        rtt_ms = (reply_ns - t0_ns) / 1_000_000.0
        if self._samples_ms:
            ordered = sorted(self._samples_ms)
            median = ordered[len(ordered) // 2]
            if rtt_ms > median * _OUTLIER_FACTOR:
                self._consecutive_rejects += 1
                if self._consecutive_rejects < 2:
                    return False
                # Two in a row: believe the new regime, forget the old one.
                self._samples_ms.clear()
                self._consecutive_rejects = 0
                self._samples_ms.append(rtt_ms)
                return True
        self._consecutive_rejects = 0
        self._samples_ms.append(rtt_ms)
        return True

    @property
    def rtt_ms(self) -> float:
        """Mean of the accepted samples; 0.0 with no accepted sample."""
        return sum(self._samples_ms) / len(self._samples_ms) if self._samples_ms else 0.0

    @property
    def measured(self) -> bool:
        return bool(self._samples_ms)


@dataclass
class VelocityPredictor:
    """Alpha-beta filter per target: position + velocity smoothing.

    The predictor extrapolates by ``latency_ms`` — the measured end-to-end
    delay — so the assist aims at where the target *will be*, not where the
    camera saw it (the ReaSnow failure mode).
    """

    alpha: float = 0.5
    beta: float = 0.2

    _x: float = 0.0
    _y: float = 0.0
    _vx: float = 0.0
    _vy: float = 0.0
    _initialized: bool = False

    def observe(self, x: float, y: float, dt_s: float) -> None:
        if dt_s <= 0:
            raise LatencyError("dt_s must be positive")
        if not self._initialized:
            self._x, self._y = x, y
            self._vx = self._vy = 0.0
            self._initialized = True
            return
        # Predict, then correct (alpha-beta).
        predicted_x = self._x + self._vx * dt_s
        predicted_y = self._y + self._vy * dt_s
        residual_x = x - predicted_x
        residual_y = y - predicted_y
        self._x = predicted_x + self.alpha * residual_x
        self._y = predicted_y + self.alpha * residual_y
        self._vx += (self.beta / dt_s) * residual_x
        self._vy += (self.beta / dt_s) * residual_y

    def extrapolate(self, latency_ms: float) -> tuple[float, float]:
        if latency_ms < 0:
            raise LatencyError("latency_ms must be non-negative")
        seconds = latency_ms / 1000.0
        return (self._x + self._vx * seconds, self._y + self._vy * seconds)


@dataclass(frozen=True)
class LatencyBreakdown:
    """Segments of the end-to-end budget with provenance.

    ``capture_to_detect_ms`` is measured when the caller has both the camera
    sensor timestamp and the detection-complete timestamp; ``transport_ms``
    is measured (RTT/2) only when a round trip completed; everything else is
    explicitly marked assumed so the debug surface can show the difference.
    """

    capture_to_detect_ms: float
    capture_to_detect_measured: bool
    transport_ms: float
    transport_measured: bool
    assumed_total_ms: float

    @property
    def compensation_ms(self) -> float:
        """The value the predictor extrapolates by: measured segments when
        available, the assumed budget for the rest — never zero by omission."""
        measured = 0.0
        if self.capture_to_detect_measured:
            measured += self.capture_to_detect_ms
        if self.transport_measured:
            measured += self.transport_ms
        if self.capture_to_detect_measured and self.transport_measured:
            return measured
        return max(measured, self.assumed_total_ms)


def breakdown(
    *,
    camera_to_detect_ms: float | None,
    rtt: RoundTrip,
    assumed_total_ms: float = 80.0,
) -> LatencyBreakdown:
    """Builds a breakdown from what was actually measured."""
    if camera_to_detect_ms is not None and camera_to_detect_ms < 0:
        raise LatencyError("camera_to_detect_ms must be non-negative")
    if assumed_total_ms < 0:
        raise LatencyError("assumed_total_ms must be non-negative")
    measured_capture = camera_to_detect_ms is not None
    measured_transport = rtt.measured
    return LatencyBreakdown(
        capture_to_detect_ms=camera_to_detect_ms or 0.0,
        capture_to_detect_measured=measured_capture,
        transport_ms=rtt.rtt_ms / 2.0,
        transport_measured=measured_transport,
        assumed_total_ms=assumed_total_ms,
    )
