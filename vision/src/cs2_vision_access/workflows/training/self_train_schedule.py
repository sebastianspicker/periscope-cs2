"""Shared conf schedule + early-stop streak helpers for multi-iter self-train.

Single source of truth for auto-train and remote autonomous loops so conf
bands and plateau early-stop stay consistent across entry points.
"""

from __future__ import annotations


def scheduled_self_train_conf(
    base: float,
    iteration: int,
    *,
    enabled: bool = True,
    cap: float = 0.85,
    growth: float = 0.05,
) -> float:
    """Raise self-train conf over iterations when *enabled*.

    Formula: ``min(cap, base * (1 + growth * (iteration - 1)))``.
    Iteration is 1-based. When *enabled* is False, returns *base* unchanged.
    """
    base_f = float(base)
    if not enabled:
        return base_f
    it = max(1, int(iteration))
    return min(float(cap), base_f * (1.0 + float(growth) * (it - 1)))


def resolve_conf_bands(
    conf_threshold: float,
    *,
    conf_low: float | None = None,
    conf_high: float | None = None,
    iteration: int = 1,
    conf_schedule: bool = False,
) -> tuple[float, float, float]:
    """Return ``(conf, conf_low, conf_high)`` with optional per-iter schedule.

    SSOT policy for auto + remote multi-iter self-train:

    1. Schedule ``conf_threshold`` first when ``conf_schedule`` is True
       (via :func:`scheduled_self_train_conf`).
    2. ``conf_high`` defaults to the scheduled conf.
    3. ``conf_low`` defaults to ``0.7 *`` scheduled conf.
    4. When ``conf_low`` / ``conf_high`` are explicitly set, those bases are
       scheduled the same way (match remote behavior for explicit bases).
    5. ``conf_low`` is clamped so it never exceeds ``conf_high``.
    """
    conf = scheduled_self_train_conf(conf_threshold, iteration, enabled=bool(conf_schedule))
    if conf_high is not None:
        high = scheduled_self_train_conf(float(conf_high), iteration, enabled=bool(conf_schedule))
    else:
        high = conf
    if conf_low is not None:
        low = scheduled_self_train_conf(float(conf_low), iteration, enabled=bool(conf_schedule))
    else:
        low = conf * 0.7
    low = min(high, low)
    return conf, low, high


def growth_plateau_update(
    plateau: int,
    *,
    accepted: int,
    stop_on_no_growth: bool,
    max_plateau_iters: int,
) -> tuple[int, str | None]:
    """Update consecutive zero-accept streak; optionally request early stop.

    Returns ``(new_plateau, stop_reason or None)``.

    - If ``accepted > 0``: plateau resets to 0, no stop.
    - If ``accepted <= 0`` and ``stop_on_no_growth``: plateau increments; when
      plateau >= ``max_plateau_iters``, stop reason is ``\"plateau\"`` when
      ``max_plateau_iters > 1`` else ``\"no_growth\"``.
    - If ``accepted <= 0`` and not ``stop_on_no_growth``: plateau unchanged.
    """
    if int(accepted) > 0:
        return 0, None
    if not stop_on_no_growth:
        return int(plateau), None
    new_plateau = int(plateau) + 1
    max_p = max(1, int(max_plateau_iters))
    if new_plateau >= max_p:
        reason = "plateau" if max_p > 1 else "no_growth"
        return new_plateau, reason
    return new_plateau, None


__all__ = [
    "growth_plateau_update",
    "resolve_conf_bands",
    "scheduled_self_train_conf",
]
