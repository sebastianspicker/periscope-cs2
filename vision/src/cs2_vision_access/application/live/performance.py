"""Per-device performance profiles and present-latency telemetry (ledger V3).

Adapted from the research ledger: MS-Mesh's honesty rule ("device preference
is advisory: show the actual renderer") and per-device cadence profiles, plus
a bounded latency window for backends that can measure present latency.

Rules carried over:

- **Profiles are advisory.** Selection is derived from the renderer string
  the runtime *actually reports*, never from what was requested; the
  description returned to users contains the real string.
- **Explicit overrides win.** A CLI/config-provided cadence overrides the
  profile value (the house A11 lesson: silently letting a profile overwrite
  an explicit setting compares the wrong things).
- **Bounded collections.** Every timing window is a bounded deque (the B13
  lesson: unbounded per-frame lists grow ~100 MB over an 8 h session).
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

from cs2_vision_access.domain.statistics import percentile

_MAX_WINDOW = 10_000


@dataclass(frozen=True)
class DeviceProfile:
    """Advisory cadence/robustness settings for a device class.

    ``state_reader_hz`` is the state-reader cadence hint; ``frame_skip``
    and ``temporal_min_consecutive`` mirror the live-pipeline knobs they
    tune. All three are defaults, not commands.
    """

    name: str
    state_reader_hz: float
    frame_skip: int
    temporal_min_consecutive: int
    notes: str = ""


# Keyword-ordered classification: first match wins; the list is deliberately
# explicit so a new vendor is an added row, not a guessed regex.
_PROFILE_TABLE: tuple[tuple[str, DeviceProfile], ...] = (
    (
        "apple",
        DeviceProfile(
            name="apple-silicon",
            state_reader_hz=60.0,
            frame_skip=2,
            temporal_min_consecutive=2,
            notes="Metal/Apple GPU: unified memory, inference usually keeps up",
        ),
    ),
    (
        "nvidia",
        DeviceProfile(
            name="nvidia-dedicated",
            state_reader_hz=60.0,
            frame_skip=2,
            temporal_min_consecutive=2,
            notes="CUDA/ONNX-DirectML path well supported",
        ),
    ),
    (
        "amd",
        DeviceProfile(
            name="amd-dedicated",
            state_reader_hz=45.0,
            frame_skip=3,
            temporal_min_consecutive=2,
            notes="DirectML path; slightly conservative cadence",
        ),
    ),
    (
        "intel",
        DeviceProfile(
            name="intel-integrated",
            state_reader_hz=30.0,
            frame_skip=4,
            temporal_min_consecutive=3,
            notes="Integrated GPU: keep the temporal filter patient",
        ),
    ),
    (
        "llvmpipe",
        DeviceProfile(
            name="software-renderer",
            state_reader_hz=15.0,
            frame_skip=6,
            temporal_min_consecutive=4,
            notes="Software rendering: minimal cadence, expect drops",
        ),
    ),
)

FALLBACK_PROFILE = DeviceProfile(
    name="unknown-device",
    state_reader_hz=30.0,
    frame_skip=4,
    temporal_min_consecutive=3,
    notes="Unrecognized renderer string; conservative defaults",
)


def classify_renderer(renderer: str) -> DeviceProfile:
    """Advisory profile for the *actually reported* renderer string."""
    lowered = renderer.lower()
    for keyword, profile in _PROFILE_TABLE:
        if keyword in lowered:
            return profile
    return FALLBACK_PROFILE


def select_profile(
    renderer: str,
    overrides: dict[str, float | int] | None = None,
) -> DeviceProfile:
    """Profile for a renderer with explicit operator overrides applied.

    ``overrides`` keys are profile field names (``state_reader_hz``,
    ``frame_skip``, ``temporal_min_consecutive``); unknown keys are rejected
    rather than ignored, a typo must not silently disable an override.
    """
    profile = classify_renderer(renderer)
    if not overrides:
        return profile
    known = {"state_reader_hz", "frame_skip", "temporal_min_consecutive"}
    unknown = sorted(set(overrides) - known)
    if unknown:
        raise ValueError(f"unknown profile overrides: {unknown}")
    return DeviceProfile(
        name=profile.name,
        state_reader_hz=float(overrides.get("state_reader_hz", profile.state_reader_hz)),
        frame_skip=int(overrides.get("frame_skip", profile.frame_skip)),
        temporal_min_consecutive=int(
            overrides.get("temporal_min_consecutive", profile.temporal_min_consecutive)
        ),
        notes=profile.notes,
    )


def describe_active(renderer: str, profile: DeviceProfile) -> str:
    """User-facing description that always names the *actual* renderer.

    The honesty rule: "Device preference is advisory, settings show the
    actual renderer." A profile name is never a substitute for the real
    string.
    """
    return f"renderer: {renderer!r} (profile {profile.name}, advisory only), {profile.notes}"


@dataclass
class LatencyWindow:
    """Bounded paired-sample window for present latency.

    Backends that can measure when a captured frame was captured vs when it
    was actually presented push ``(capture_ns, present_ns)`` pairs; the
    window reports p50/p95 of the deltas. Non-monotonic or negative deltas
    are rejected (a clock glitch must not poison the percentile).
    """

    maxlen: int = _MAX_WINDOW
    _deltas_ms: deque[float] = field(default_factory=lambda: deque())

    def __post_init__(self) -> None:
        if self.maxlen < 1:
            raise ValueError("maxlen must be positive")
        # The field default cannot reference the instance maxlen; rebuild it.
        self._deltas_ms = deque(self._deltas_ms, maxlen=self.maxlen)

    def record(self, capture_ns: int, present_ns: int) -> bool:
        if present_ns < capture_ns:
            return False
        self._deltas_ms.append((present_ns - capture_ns) / 1_000_000.0)
        return True

    @property
    def count(self) -> int:
        return len(self._deltas_ms)

    def percentile(self, fraction: float) -> float:
        """Linear-interpolation percentile of the recorded deltas (0.0 when
        empty), the same estimator the live diagnostics use, so profile
        tuning and latency reporting never disagree about "p95"."""
        if not self._deltas_ms:
            return 0.0
        if not 0.0 <= fraction <= 1.0:
            raise ValueError("fraction must be in [0, 1]")
        return percentile(list(self._deltas_ms), round(fraction * 100))

    def summary(self) -> tuple[float, float]:
        return (self.percentile(0.50), self.percentile(0.95))
