"""Output paths for the computed assist (ledger L8).

Three ranked paths, decided by feature detection — never assumed:

1. ``hid_gadget`` — the phone presents itself as a USB-HID mouse (Android
   10+ configfs USB gadget, kernel-dependent). No PC software at all; the
   cable carries both the L6 target stream and the HID reports. Feature
   detection is mandatory: support varies per device.
2. ``pc_agent`` — a small opt-in PC-side receiver translating targets to
   OS input events. Same actuation class as the audit-excluded presenter
   area: lab-only, never default.
3. ``targets_only`` — the PC side owns all motion policy; the phone only
   publishes targets (degraded but always available).

The pure half here: the output-path decision ladder over detected feature
flags, and the HID delta shaper (accumulation, clamping to the signed-byte
report range, rate shaping). The configfs plumbing and the OS-event agent
are runtime work behind these interfaces (see `docs/OUTPUT-SUPPORT.md`).
"""

from __future__ import annotations

import math
from collections.abc import Iterator
from dataclasses import dataclass, field
from enum import StrEnum

HID_AXIS_MIN = -127
HID_AXIS_MAX = 127


class OutputPath(StrEnum):
    HID_GADGET = "hid_gadget"
    PC_AGENT = "pc_agent"
    TARGETS_ONLY = "targets_only"


@dataclass(frozen=True)
class OutputFeatures:
    """What the runtime actually detected on this phone/PC pair."""

    hid_gadget_available: bool = False
    pc_agent_reachable: bool = False

    def validate(self) -> None:
        # Both flags absent is legal (targets-only), no flag combination is.
        return


def select_output_path(features: OutputFeatures) -> OutputPath:
    """The decision ladder, in priority order (ledger L8).

    Never raises: ``targets_only`` is the always-available degradation, and
    the caller surfaces which path was chosen (L8 acceptance: "gadget
    detection matrix documented per tested device").
    """
    features.validate()
    if features.hid_gadget_available:
        return OutputPath.HID_GADGET
    if features.pc_agent_reachable:
        return OutputPath.PC_AGENT
    return OutputPath.TARGETS_ONLY


@dataclass
class HidDeltaShaper:
    """Accumulates desired motion and emits rate-shaped HID reports.

    The phone sends relative mouse deltas (what HID mice are — no absolute
    positioning assumptions). Reports carry signed bytes per axis, so a
    desired motion beyond one report's range is emitted across several
    reports; ``max_counts_per_report`` additionally rate-shapes so a large
    correction never lands as one violent jump (the humanizer's floor).
    """

    max_counts_per_report: int = 40
    _acc_x: float = field(default=0.0)
    _acc_y: float = field(default=0.0)

    def __post_init__(self) -> None:
        if self.max_counts_per_report < 1:
            raise ValueError("max_counts_per_report must be >= 1")

    def add(self, dx: float, dy: float) -> None:
        if not math.isfinite(dx) or not math.isfinite(dy):
            raise ValueError("HID deltas must be finite")
        self._acc_x += dx
        self._acc_y += dy

    @property
    def pending(self) -> tuple[float, float]:
        return (self._acc_x, self._acc_y)

    def reports(self) -> Iterator[tuple[int, int]]:
        """Drains the accumulator into (dx, dy) report tuples.

        Each axis is clamped to the HID signed-byte range AND to
        ``max_counts_per_report``; leftover motion stays accumulated for the
        next report.
        """
        while self._acc_x != 0.0 or self._acc_y != 0.0:
            if not (math.isfinite(self._acc_x) and math.isfinite(self._acc_y)):
                # Defensive: a non-finite residual would loop forever.
                self._acc_x = 0.0 if not math.isfinite(self._acc_x) else self._acc_x
                self._acc_y = 0.0 if not math.isfinite(self._acc_y) else self._acc_y
            dx = self._clip(self._acc_x)
            dy = self._clip(self._acc_y)
            self._acc_x -= dx
            self._acc_y -= dy
            if dx == 0 and dy == 0:
                break  # sub-count residuals stay accumulated
            yield (int(dx), int(dy))

    def _clip(self, value: float) -> float:
        axis_clamped = max(float(HID_AXIS_MIN), min(float(HID_AXIS_MAX), value))
        magnitude = abs(axis_clamped)
        if magnitude > self.max_counts_per_report:
            sign = 1.0 if axis_clamped >= 0 else -1.0
            axis_clamped = sign * self.max_counts_per_report
        return float(int(axis_clamped))


def encode_hid_report(dx: int, dy: int, buttons: int = 0) -> bytes:
    """A standard 3-byte relative mouse HID report (button, dx, dy).

    Values must already be in range — the shaper guarantees that; this
    function refuses anything else so a clamp bug cannot become a wrapped
    axis (a mouse teleporting across the screen).
    """
    if not HID_AXIS_MIN <= dx <= HID_AXIS_MAX:
        raise ValueError(f"dx out of HID range: {dx}")
    if not HID_AXIS_MIN <= dy <= HID_AXIS_MAX:
        raise ValueError(f"dy out of HID range: {dy}")
    if not 0 <= buttons <= 0xFF:
        raise ValueError("buttons must fit a byte")
    return bytes((buttons, dx & 0xFF, dy & 0xFF))
