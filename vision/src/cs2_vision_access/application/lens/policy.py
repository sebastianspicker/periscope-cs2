"""Aim-assist policy (ledger L9) — the decision layer between targets and
motion deltas. **Pure**: it computes numbers; it never moves a cursor. The
L8 output paths consume these deltas under lab-only opt-in flags.

Parameter space (in priority order, per the ledger):

1. FOV gate — radius around the current aim inside which targets are
   eligible (targets outside the gate are invisible to the assist).
2. Target priority — angular/screen distance to the crosshair, with sticky
   target hysteresis so the assist does not flicker between two enemies.
3. Smoothing — exponential response to the *compensated* error (L7's
   predictor output, not raw error), capped per-step velocity, dead-zone.
4. Engagement conditions — only-while-held is the caller's concern; the
   policy adds a cooldown after large corrections so a single frame glitch
   cannot produce a snap.

Defaults are deliberately subtle (the X4 context: subtle is the only safe
default; anything else is a rage-cheat demo, not an accessibility aid).
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.application.lens.transport import TargetBox


class PolicyError(ValueError):
    """Policy parameters violated the contract."""


@dataclass(frozen=True)
class AssistPolicy:
    fov_px: float = 120.0  # eligibility radius around aim
    smoothing: float = 0.35  # 0 = snap, 1 = very soft
    max_step_px: float = 18.0  # per-frame velocity cap
    dead_zone_px: float = 2.0  # ignore residual jitter
    sticky_switch_px: float = 12.0  # a new target must beat the
    # current one by this margin before the lock switches
    cooldown_frames_after_large_step: int = 3
    large_step_px: float = 60.0

    def __post_init__(self) -> None:
        if self.fov_px <= 0:
            raise PolicyError("fov_px must be positive")
        if not 0.0 <= self.smoothing <= 1.0:
            raise PolicyError("smoothing must be in [0, 1]")
        if self.max_step_px <= 0:
            raise PolicyError("max_step_px must be positive")
        if self.dead_zone_px < 0:
            raise PolicyError("dead_zone_px must be non-negative")
        if self.sticky_switch_px < 0:
            raise PolicyError("sticky_switch_px must be non-negative")
        if self.cooldown_frames_after_large_step < 0:
            raise PolicyError("cooldown must be non-negative")


@dataclass
class PolicyState:
    """Mutable per-session state: current lock and cooldown counter."""

    locked_target_id: int | None = None
    cooldown: int = 0


def select_target(
    boxes: Sequence[TargetBox],
    crosshair: tuple[float, float],
    policy: AssistPolicy,
    state: PolicyState,
    target_ids: Sequence[int] | None = None,
) -> tuple[int, TargetBox] | None:
    """FOV gate + sticky priority. Returns (id, box) or None.

    Target ids default to enumeration order; a locked target keeps the lock
    unless it leaves the FOV or another center comes closer by more than
    ``sticky_switch_px``.
    """
    if target_ids is not None and len(target_ids) != len(boxes):
        raise PolicyError("target_ids must parallel boxes")
    ids = list(target_ids) if target_ids is not None else list(range(len(boxes)))

    def distance(box: TargetBox) -> float:
        return math.hypot(box.cx - crosshair[0], box.cy - crosshair[1])

    eligible = [
        (distance(box), id_, box)
        for id_, box in zip(ids, boxes, strict=True)
        if distance(box) <= policy.fov_px
    ]
    if not eligible:
        state.locked_target_id = None
        return None

    if state.locked_target_id is not None:
        locked = next(
            ((d, i, b) for d, i, b in eligible if i == state.locked_target_id),
            None,
        )
        best = min(eligible, key=lambda item: item[0])
        if locked is not None and locked[0] <= best[0] + policy.sticky_switch_px:
            return (locked[1], locked[2])
        # Locked target left the FOV or was decisively out-sticked: re-select.

    best_distance, best_id, best_box = min(eligible, key=lambda item: item[0])
    state.locked_target_id = best_id
    return (best_id, best_box)


def motion_delta(
    target_center: tuple[float, float],
    crosshair: tuple[float, float],
    policy: AssistPolicy,
    state: PolicyState,
) -> tuple[float, float]:
    """Smoothed, capped, dead-zoned correction toward the target center.

    The input error must already be latency-compensated (L7); this function
    deliberately takes a center, not a raw box, so compensating twice is a
    caller error rather than a silent double-correction.
    """
    if state.cooldown > 0:
        state.cooldown -= 1
        return (0.0, 0.0)

    dx = target_center[0] - crosshair[0]
    dy = target_center[1] - crosshair[1]
    distance = math.hypot(dx, dy)
    if distance <= policy.dead_zone_px:
        return (0.0, 0.0)

    # Cap the step, then soften: the cap bounds velocity, the smoothing
    # fraction shapes the approach (0 → instant to the cap, 1 → crawl).
    scale = min(1.0, policy.max_step_px / distance)
    step_x = dx * scale
    step_y = dy * scale
    soft = 1.0 - policy.smoothing
    step_x *= soft
    step_y *= soft

    if math.hypot(step_x, step_y) >= policy.large_step_px:
        state.cooldown = policy.cooldown_frames_after_large_step
    return (step_x, step_y)
