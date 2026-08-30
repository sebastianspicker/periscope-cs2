"""Opt-in temporal stability filter (anti-flash) with optional hold-last-mask.

By default this module only **suppresses** masks that have not yet been stable
for ``min_consecutive_frames`` consecutive frames; it never invents geometry for
frames with no prediction.

When ``TemporalStabilityConfig.hold_last_mask`` is enabled together with a
positive ``max_dropout_frames``, a **previously stabilized** mask may be carried
forward across short detector dropouts. Only the exact stored polygon of a
stable detection is re-emitted (geometry is never invented), and the carry
window is bounded by ``max_dropout_frames``. Masks that never reached stability
are never held.

Default pipeline behaviour is unchanged when ``TemporalStabilityConfig.enabled``
is false (identity passthrough).
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.domain.predictions import InstanceMask, Point

# Association heuristics (no track IDs; consecutive-frame only).
DEFAULT_IOU_THRESHOLD = 0.1
DEFAULT_CENTROID_MATCH_PX = 64.0


@dataclass(frozen=True)
class TemporalStabilityConfig:
    """Configuration for temporal filtering.

    ``enabled`` defaults to false so the video loop stays frame-sync discard only.
    ``min_consecutive_frames`` is the number of contiguous matched appearances
    required before a mask is drawn (default 2 when the feature is enabled).

    ``hold_last_mask`` opts into carry-forward (hold-last-mask): masks that have
    already reached stability may be re-emitted across short detector dropouts,
    bounded by ``max_dropout_frames`` consecutive frames (0 disables
    carry-forward, keeping suppress-only behaviour). ``hold_last_mask`` only has
    an effect when ``enabled`` is true; when ``enabled`` is false the filter is
    an identity passthrough regardless.
    """

    enabled: bool = False
    min_consecutive_frames: int = 2
    hold_last_mask: bool = False
    max_dropout_frames: int = 0

    def __post_init__(self) -> None:
        if isinstance(self.min_consecutive_frames, bool) or not isinstance(
            self.min_consecutive_frames, int
        ):
            raise ValueError("min_consecutive_frames must be an integer")
        if self.min_consecutive_frames < 1:
            raise ValueError("min_consecutive_frames must be >= 1")
        if isinstance(self.max_dropout_frames, bool) or not isinstance(
            self.max_dropout_frames, int
        ):
            raise ValueError("max_dropout_frames must be an integer")
        if self.max_dropout_frames < 0:
            raise ValueError("max_dropout_frames must be >= 0")


@dataclass(frozen=True)
class TemporalDiagnostics:
    """Per-frame filter counts (raw predictions in, drawn candidates out)."""

    suppressed_count: int
    passed_count: int
    held_count: int = 0

    def __post_init__(self) -> None:
        if self.suppressed_count < 0 or self.passed_count < 0 or self.held_count < 0:
            raise ValueError("diagnostics counts must be non-negative")


@dataclass(frozen=True)
class _Observation:
    """Ephemeral association state for one prediction on a previous frame.

    ``frame_index`` / ``polygon`` / ``confidence`` / ``class_name`` preserve the
    last-known detector output so a stable observation can be re-emitted
    verbatim when hold-last-mask is active. ``frames_since_seen`` counts
    consecutive frames without a match; ``stable`` is true once
    ``consecutive >= min_consecutive_frames``.
    """

    class_id: int
    centroid: tuple[float, float]
    bbox: tuple[float, float, float, float]
    consecutive: int
    frame_index: int
    polygon: tuple[Point, ...]
    frames_since_seen: int
    stable: bool
    confidence: float
    class_name: str


def _centroid(polygon: Sequence[Point]) -> tuple[float, float]:
    total_x = 0.0
    total_y = 0.0
    for x, y in polygon:
        total_x += float(x)
        total_y += float(y)
    count = float(len(polygon))
    return total_x / count, total_y / count


def _bbox(polygon: Sequence[Point]) -> tuple[float, float, float, float]:
    xs = [float(x) for x, _ in polygon]
    ys = [float(y) for _, y in polygon]
    return min(xs), min(ys), max(xs), max(ys)


def _bbox_iou(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> float:
    ax0, ay0, ax1, ay1 = first
    bx0, by0, bx1, by1 = second
    ix0 = max(ax0, bx0)
    iy0 = max(ay0, by0)
    ix1 = min(ax1, bx1)
    iy1 = min(ay1, by1)
    iw = max(0.0, ix1 - ix0)
    ih = max(0.0, iy1 - iy0)
    inter = iw * ih
    if inter <= 0.0:
        return 0.0
    area_a = max(0.0, ax1 - ax0) * max(0.0, ay1 - ay0)
    area_b = max(0.0, bx1 - bx0) * max(0.0, by1 - by0)
    union = area_a + area_b - inter
    if union <= 0.0:
        return 0.0
    return inter / union


def _associate(
    previous: Sequence[_Observation],
    current: Sequence[tuple[int, tuple[float, float], tuple[float, float, float, float]]],
    *,
    iou_threshold: float,
    centroid_match_px: float,
) -> dict[int, int]:
    """Greedy one-to-one match: current index → previous index.

    Same class only. A pair is a candidate when bbox IoU ≥ threshold **or**
    centroid distance ≤ ``centroid_match_px``. Prefer higher IoU, then closer
    centroid.
    """
    candidates: list[tuple[float, float, int, int]] = []
    for cur_index, (class_id, centroid, bbox) in enumerate(current):
        for prev_index, prev in enumerate(previous):
            if prev.class_id != class_id:
                continue
            iou = _bbox_iou(prev.bbox, bbox)
            distance = math.hypot(
                centroid[0] - prev.centroid[0],
                centroid[1] - prev.centroid[1],
            )
            if iou >= iou_threshold or distance <= centroid_match_px:
                # Sort key: higher IoU first, then smaller distance.
                candidates.append((-iou, distance, cur_index, prev_index))
    candidates.sort()
    matched_cur: set[int] = set()
    matched_prev: set[int] = set()
    mapping: dict[int, int] = {}
    for _neg_iou, _distance, cur_index, prev_index in candidates:
        if cur_index in matched_cur or prev_index in matched_prev:
            continue
        matched_cur.add(cur_index)
        matched_prev.add(prev_index)
        mapping[cur_index] = prev_index
    return mapping


class SuppressOnlyTemporalPolicy:
    """Drop unstable detections; optionally hold last-known geometry across dropouts.

    Association is a consecutive-frame heuristic (bbox IoU / centroid proximity
    within class). There are no persistent track IDs. When ``hold_last_mask`` is
    enabled together with a positive ``max_dropout_frames``, a previously
    stabilized mask's exact stored polygon may be re-emitted for up to
    ``max_dropout_frames`` consecutive frames with no matching prediction;
    unstable masks are never held and no geometry is ever invented.
    """

    def __init__(
        self,
        config: TemporalStabilityConfig | None = None,
        *,
        iou_threshold: float = DEFAULT_IOU_THRESHOLD,
        centroid_match_px: float = DEFAULT_CENTROID_MATCH_PX,
    ) -> None:
        self._config = config if config is not None else TemporalStabilityConfig()
        if (
            isinstance(iou_threshold, bool)
            or not isinstance(iou_threshold, (int, float))
            or not math.isfinite(float(iou_threshold))
            or not 0.0 <= float(iou_threshold) <= 1.0
        ):
            raise ValueError("iou_threshold must be a finite number in [0, 1]")
        if (
            isinstance(centroid_match_px, bool)
            or not isinstance(centroid_match_px, (int, float))
            or not math.isfinite(float(centroid_match_px))
            or float(centroid_match_px) <= 0
        ):
            raise ValueError("centroid_match_px must be a finite positive number")
        self._iou_threshold = float(iou_threshold)
        self._centroid_match_px = float(centroid_match_px)
        self._previous: tuple[_Observation, ...] = ()
        self._previous_frame: int | None = None

    @property
    def config(self) -> TemporalStabilityConfig:
        return self._config

    def filter(
        self,
        masks: Sequence[InstanceMask],
        *,
        frame_index: int,
    ) -> tuple[tuple[InstanceMask, ...], TemporalDiagnostics]:
        """Return masks stable for ``min_consecutive_frames``, plus held masks.

        Held masks (carried-forward stable geometry) are appended after the
        current frame's passed masks and never alter passed-mask identity. When
        disabled, returns the input masks unchanged (identity).
        """
        if isinstance(frame_index, bool) or not isinstance(frame_index, int) or frame_index < 0:
            raise ValueError("frame_index must be a non-negative integer")

        current_masks = tuple(masks)
        if not self._config.enabled:
            return current_masks, TemporalDiagnostics(
                suppressed_count=0,
                passed_count=len(current_masks),
            )

        hold_enabled = self._config.hold_last_mask and self._config.max_dropout_frames > 0
        contiguous = self._previous_frame is not None and frame_index == self._previous_frame + 1
        previous = self._previous if contiguous else ()

        geometry: list[tuple[int, tuple[float, float], tuple[float, float, float, float]]] = []
        for mask in current_masks:
            geometry.append((mask.class_id, _centroid(mask.polygon), _bbox(mask.polygon)))

        mapping = _associate(
            previous,
            geometry,
            iou_threshold=self._iou_threshold,
            centroid_match_px=self._centroid_match_px,
        )

        min_n = self._config.min_consecutive_frames
        passed: list[InstanceMask] = []
        held: list[InstanceMask] = []
        next_previous: list[_Observation] = []
        for index, mask in enumerate(current_masks):
            class_id, centroid, bbox = geometry[index]
            if index in mapping:
                matched = previous[mapping[index]]
                consecutive = matched.consecutive + 1
            else:
                consecutive = 1
            next_previous.append(
                _Observation(
                    class_id=class_id,
                    centroid=centroid,
                    bbox=bbox,
                    consecutive=consecutive,
                    frame_index=frame_index,
                    polygon=mask.polygon,
                    frames_since_seen=0,
                    stable=consecutive >= min_n,
                    confidence=mask.confidence,
                    class_name=mask.class_name,
                )
            )
            # Pass through the original mask object fields unchanged so
            # frame_index and geometry stay bound to the detector output.
            if consecutive >= min_n:
                passed.append(mask)

        if hold_enabled:
            matched_previous: set[int] = set(mapping.values())
            for prev_index, prev in enumerate(previous):
                if prev_index in matched_previous or not prev.stable:
                    continue
                if prev.frames_since_seen < self._config.max_dropout_frames:
                    held.append(self._held_mask(prev, frame_index))
                    next_previous.append(
                        _Observation(
                            class_id=prev.class_id,
                            centroid=prev.centroid,
                            bbox=prev.bbox,
                            consecutive=prev.consecutive,
                            frame_index=prev.frame_index,
                            polygon=prev.polygon,
                            frames_since_seen=prev.frames_since_seen + 1,
                            stable=True,
                            confidence=prev.confidence,
                            class_name=prev.class_name,
                        )
                    )

        self._previous = tuple(next_previous)
        self._previous_frame = frame_index
        returned = passed + held
        return tuple(returned), TemporalDiagnostics(
            suppressed_count=len(current_masks) - len(passed),
            passed_count=len(passed),
            held_count=len(held),
        )

    def _held_mask(self, observation: _Observation, frame_index: int) -> InstanceMask:
        """Re-emit the exact stored geometry of a previously stabilized mask.

        Only ``frame_index`` changes (to the current frame); the polygon,
        confidence, class_id, and class_name come verbatim from the stored
        observation. This is the documented hold-last-mask behaviour and never
        invents geometry.
        """
        return InstanceMask(
            frame_index=frame_index,
            polygon=observation.polygon,
            confidence=observation.confidence,
            class_id=observation.class_id,
            class_name=observation.class_name,
        )
