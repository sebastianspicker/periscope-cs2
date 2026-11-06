"""CV-as-validator agreement harness (ledger V7 — research spike).

Research question (docs/CV-VALIDATOR-SPIKE.md): can a second, cheap
screen-space CV pass *validate* the primary pipeline's per-frame instance
set — without ever generating input events? This module is the passive
measurement half: per-frame agreement between a reference instance set and a
validator instance set, as bounded running statistics.

Design rules:

- **Passive only.** Nothing here moves a cursor, sends input, or writes to
  any process. The module computes numbers; the spike report interprets them.
- **Cheap on purpose.** Agreement uses axis-aligned bounding-box IoU, not
  mask IoU — a validator that costs as much as the pipeline it validates is
  pointless.
- **Bounded.** The history window is a bounded deque (house B13 rule).
"""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field

from cs2_vision_access.domain.predictions import InstanceMask, Point

_DEFAULT_WINDOW = 2048


class ValidatorError(ValueError):
    """Validator inputs violated the contract."""


def _bbox(polygon: Sequence[Point]) -> tuple[float, float, float, float]:
    xs = [point[0] for point in polygon]
    ys = [point[1] for point in polygon]
    return (min(xs), min(ys), max(xs), max(ys))


def bbox_iou(first: Sequence[Point], second: Sequence[Point]) -> float:
    """IoU of two polygons' axis-aligned bounding boxes (0.0 on empty union)."""
    if not first or not second:
        return 0.0
    ax0, ay0, ax1, ay1 = _bbox(first)
    bx0, by0, bx1, by1 = _bbox(second)
    inter_w = min(ax1, bx1) - max(ax0, bx0)
    inter_h = min(ay1, by1) - max(ay0, by0)
    if inter_w <= 0.0 or inter_h <= 0.0:
        return 0.0
    intersection = inter_w * inter_h
    area_a = (ax1 - ax0) * (ay1 - ay0)
    area_b = (bx1 - bx0) * (by1 - by0)
    union = area_a + area_b - intersection
    if union <= 0.0:
        return 0.0
    return intersection / union


@dataclass(frozen=True)
class FrameAgreement:
    """One frame's comparison result."""

    frame_index: int
    reference_count: int
    validator_count: int
    matched: int
    missed: int  # in reference, not matched by the validator
    spurious: int  # validator detections with no reference partner
    mean_iou: float

    @property
    def agreement_rate(self) -> float:
        if self.reference_count == 0:
            return 1.0 if self.validator_count == 0 else 0.0
        return self.matched / self.reference_count


@dataclass
class ValidatorReport:
    """Bounded running agreement statistics."""

    window: int = _DEFAULT_WINDOW
    frames: deque[FrameAgreement] = field(default_factory=lambda: deque())

    def __post_init__(self) -> None:
        if self.window < 1:
            raise ValidatorError("window must be positive")
        self.frames = deque(self.frames, maxlen=self.window)

    def record_frame(
        self,
        frame_index: int,
        reference: Iterable[InstanceMask],
        validator: Iterable[InstanceMask],
        *,
        iou_threshold: float = 0.3,
    ) -> FrameAgreement:
        """Greedy one-to-one bbox matching by descending IoU.

        Class-agnostic by design: the spike asks "does the validator see the
        same *objects*", not "does it reproduce the taxonomy".
        """
        if not 0.0 < iou_threshold <= 1.0:
            raise ValidatorError("iou_threshold must be in (0, 1]")
        reference_list = list(reference)
        validator_list = list(validator)

        frame_indices = {instance.frame_index for instance in reference_list} | {
            instance.frame_index for instance in validator_list
        }
        if frame_indices and frame_index not in frame_indices:
            raise ValidatorError("instances must belong to the frame under comparison")

        candidates: list[tuple[float, int, int]] = []
        for r_index, ref in enumerate(reference_list):
            for v_index, val in enumerate(validator_list):
                iou = bbox_iou(ref.polygon, val.polygon)
                if iou >= iou_threshold:
                    candidates.append((iou, r_index, v_index))
        candidates.sort(key=lambda item: (-item[0], item[1], item[2]))

        used_r: set[int] = set()
        used_v: set[int] = set()
        ious: list[float] = []
        for iou, r_index, v_index in candidates:
            if r_index in used_r or v_index in used_v:
                continue
            used_r.add(r_index)
            used_v.add(v_index)
            ious.append(iou)

        agreement = FrameAgreement(
            frame_index=frame_index,
            reference_count=len(reference_list),
            validator_count=len(validator_list),
            matched=len(ious),
            missed=len(reference_list) - len(ious),
            spurious=len(validator_list) - len(ious),
            mean_iou=sum(ious) / len(ious) if ious else 0.0,
        )
        self.frames.append(agreement)
        return agreement

    def summary(self) -> dict[str, float]:
        """Bounded-window aggregates; zeros when empty (never invented)."""
        if not self.frames:
            return {
                "frames": 0,
                "agreement_rate": 0.0,
                "mean_iou": 0.0,
                "missed_per_frame": 0.0,
                "spurious_per_frame": 0.0,
            }
        total_reference = sum(frame.reference_count for frame in self.frames)
        total_matched = sum(frame.matched for frame in self.frames)
        total_missed = sum(frame.missed for frame in self.frames)
        total_spurious = sum(frame.spurious for frame in self.frames)
        scored = [frame for frame in self.frames if frame.reference_count > 0]
        return {
            "frames": float(len(self.frames)),
            "agreement_rate": (total_matched / total_reference if total_reference else 0.0),
            "mean_iou": (sum(frame.mean_iou for frame in scored) / len(scored) if scored else 0.0),
            "missed_per_frame": total_missed / len(self.frames),
            "spurious_per_frame": total_spurious / len(self.frames),
        }
