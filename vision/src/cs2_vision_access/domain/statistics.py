"""Small deterministic statistics shared by live and offline workflows."""

from __future__ import annotations


def percentile(values: list[float], p: int) -> float:
    """Return the *p*-th percentile by linear interpolation."""
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
