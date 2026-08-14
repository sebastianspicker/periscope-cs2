"""Numeric helpers for video run timing summaries."""

from __future__ import annotations


def percentile(values: list[float], p: int) -> float:
    """Return the *p*-th percentile via linear interpolation.

    Single source of truth for latency percentile summaries (video loop and
    live-pipeline diagnostics). Empty input yields ``0.0``; a single sample
    is returned as-is.
    """
    if not values:
        return 0.0
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    position = (len(ordered) - 1) * (p / 100.0)
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


# Stable private alias kept for existing imports / package re-exports.
_percentile = percentile
