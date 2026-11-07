"""Plausibility gate for live predictions (assessment-ledger item E7).

Principle: **degrade to "don't draw" instead of drawing garbage.** Before
detections reach the temporal filter, the track animator, or any renderer,
each prediction must pass sanity gates, confidence floor, polygon area
bounds, optional class allow-list, and finite coordinates. A prediction that
fails a gate is dropped with a counted reason; a dropped prediction is never
rendered.

Gate thresholds are project-configurable; the defaults here are deliberately
loose (gate disabled by default, matching the pipeline's opt-in culture) and
should be re-derived from validation data per model, not copied from any
domain-specific magic numbers.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.application.live.inference.association import bbox as polygon_bbox
from cs2_vision_access.domain.predictions import InstanceMask


@dataclass(frozen=True)
class PredictionGateConfig:
    """Configuration for the prediction plausibility gate.

    ``enabled`` defaults to false (identity passthrough). When enabled:

    - ``min_confidence``, drop detections below this detector confidence.
    - ``min_area_px``, drop detections whose bounding box area (px²) is
      below this; sub-detection specks are worse than nothing.
    - ``max_area_fraction``, drop detections whose bounding box covers more
      than this fraction of the frame; a "detection" covering the whole frame
      is a degraded read, not a player.
    - ``allowed_class_ids`` / ``allowed_class_names``, optional allow-lists
      (``None`` disables the check). An empty tuple allow-list allows nothing.
    """

    enabled: bool = False
    min_confidence: float = 0.1
    min_area_px: float = 64.0
    max_area_fraction: float = 0.9
    allowed_class_ids: tuple[int, ...] | None = None
    allowed_class_names: tuple[str, ...] | None = None

    def __post_init__(self) -> None:
        for name in ("min_confidence", "min_area_px", "max_area_fraction"):
            value = getattr(self, name)
            if (
                isinstance(value, bool)
                or not isinstance(value, (int, float))
                or not math.isfinite(float(value))
                or float(value) < 0.0
            ):
                raise ValueError(f"{name} must be a finite non-negative number")
        if not 0.0 <= float(self.min_confidence) <= 1.0:
            raise ValueError("min_confidence must be in [0, 1]")
        if float(self.max_area_fraction) <= 0.0 or float(self.max_area_fraction) > 1.0:
            raise ValueError("max_area_fraction must be in (0, 1]")
        for name in ("allowed_class_ids", "allowed_class_names"):
            value = getattr(self, name)
            if value is not None and not isinstance(value, tuple):
                raise ValueError(f"{name} must be a tuple of strings/ints or None")


@dataclass(frozen=True)
class GateDiagnostics:
    """Per-frame gate counts by drop reason."""

    passed_count: int
    dropped_low_confidence: int = 0
    dropped_small_area: int = 0
    dropped_large_area: int = 0
    dropped_class_filter: int = 0
    dropped_nonfinite: int = 0

    @property
    def dropped_count(self) -> int:
        return (
            self.dropped_low_confidence
            + self.dropped_small_area
            + self.dropped_large_area
            + self.dropped_class_filter
            + self.dropped_nonfinite
        )


def _polygon_area_px(box: tuple[float, float, float, float]) -> float:
    x0, y0, x1, y1 = box
    return max(0.0, x1 - x0) * max(0.0, y1 - y0)


def gate_predictions(
    masks: Sequence[InstanceMask],
    config: PredictionGateConfig,
    *,
    frame_width: int,
    frame_height: int,
) -> tuple[tuple[InstanceMask, ...], GateDiagnostics]:
    """Return the masks that pass every enabled gate, plus drop counts.

    Identity when ``config.enabled`` is false: the input masks are returned
    unchanged with zero drop counts.
    """
    current = tuple(masks)
    if not config.enabled:
        return current, GateDiagnostics(passed_count=len(current))

    frame_area = float(max(1, frame_width) * max(1, frame_height))
    passed: list[InstanceMask] = []
    low_confidence = small_area = large_area = class_filter = nonfinite = 0

    allowed_ids = config.allowed_class_ids
    allowed_names = config.allowed_class_names

    for mask in current:
        if not mask.confidence >= 0.0 or mask.confidence < config.min_confidence:
            low_confidence += 1
            continue
        polygon_finite = all(
            math.isfinite(float(x)) and math.isfinite(float(y)) for x, y in mask.polygon
        )
        if not polygon_finite:
            nonfinite += 1
            continue
        box = polygon_bbox(mask.polygon)
        area = _polygon_area_px(box)
        if area < config.min_area_px:
            small_area += 1
            continue
        if area > config.max_area_fraction * frame_area:
            large_area += 1
            continue
        if allowed_ids is not None and mask.class_id not in allowed_ids:
            class_filter += 1
            continue
        if allowed_names is not None and mask.class_name not in allowed_names:
            class_filter += 1
            continue
        passed.append(mask)

    return tuple(passed), GateDiagnostics(
        passed_count=len(passed),
        dropped_low_confidence=low_confidence,
        dropped_small_area=small_area,
        dropped_large_area=large_area,
        dropped_class_filter=class_filter,
        dropped_nonfinite=nonfinite,
    )
