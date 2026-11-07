"""Track smoothing and animation for the live outline overlay.

Addresses the assessment-ledger items E6 (frame-stable animated overlay
state) and E9 (hot/cold field caching on reusable track objects):

- Every detection is associated with a **persistent track** across frames
  using the same consecutive-frame heuristics as the temporal filter (bbox
  IoU / centroid proximity within class). Each track owns a stable identity
  (monotonic id), its class label, and a smoothed bounding box.
- **Jitter smoothing (E6):** the track's bounding box follows the detection
  with exponential smoothing (``1 - exp(-rate)`` per update, frame-rate
  independent integration, unlike a naive ``dt * speed``). The frame's
  polygon is then linearly re-mapped from its own raw bounding box onto the
  smoothed box, which kills box wobble without ever inventing geometry:
  every emitted vertex is an affine transform of a vertex the detector
  produced this frame.
- **Fade-in / fade-out (E6):** a new track grows from its centroid over
  ``fade_in_frames`` updates; a track that stops matching shrinks toward its
  centroid over ``fade_out_frames`` updates before it is dropped. The
  shrink/grow re-uses only polygons the detector actually produced (last
  seen for fading tracks), bounded like hold-last-mask, nothing is invented
  beyond an affine scale of seen geometry.
- **Hot/cold caching (E9):** the track object is the cache. Hot fields
  (smoothed box, polygon) refresh every matched frame; cold fields (class
  id/name, identity) persist on the track and are only rewritten on a match
  that changes them, with explicit invalidation when the class changes.

Like the temporal filter this is opt-in: ``TrackSmoothingConfig.enabled``
defaults to false and the identity behaviour is a passthrough.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from cs2_vision_access.application.live.inference.association import (
    DEFAULT_CENTROID_MATCH_PX,
    DEFAULT_IOU_THRESHOLD,
    Bbox,
    associate_greedy,
)
from cs2_vision_access.application.live.inference.association import (
    bbox as polygon_bbox,
)
from cs2_vision_access.application.live.inference.association import (
    centroid as polygon_centroid,
)
from cs2_vision_access.domain.predictions import InstanceMask, Point

_EPSILON = 1e-9
_MIN_FADE_FACTOR = 0.05


@dataclass(frozen=True)
class TrackSmoothingConfig:
    """Configuration for the track smoother/animator.

    ``enabled`` defaults to false (identity passthrough, matching the
    pipeline's opt-in culture). ``smoothing_rate`` is the exponential
    smoothing constant: higher follows detections tighter (0 disables
    smoothing entirely). ``fade_in_frames``/``fade_out_frames`` bound the
    grow/shrink animation; 0 disables that direction. ``max_gap_frames`` is
    how long a track survives without a match (identity memory).
    ``max_tracks`` bounds the track table.
    """

    enabled: bool = False
    smoothing_rate: float = 12.0
    fade_in_frames: int = 2
    fade_out_frames: int = 3
    max_gap_frames: int = 4
    max_tracks: int = 256
    # E71: a new track must match this many frames before it is emitted, so a
    # one-frame false positive never draws. 1 restores the old behaviour.
    min_confirmations: int = 2

    def __post_init__(self) -> None:
        if (
            isinstance(self.smoothing_rate, bool)
            or not isinstance(self.smoothing_rate, (int, float))
            or not math.isfinite(float(self.smoothing_rate))
            or float(self.smoothing_rate) < 0.0
        ):
            raise ValueError("smoothing_rate must be a finite non-negative number")
        for name in ("fade_in_frames", "fade_out_frames", "max_gap_frames"):
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                raise ValueError(f"{name} must be a non-negative integer")
        if isinstance(self.max_tracks, bool) or not isinstance(self.max_tracks, int):
            raise ValueError("max_tracks must be an integer")
        if self.max_tracks < 1:
            raise ValueError("max_tracks must be >= 1")
        if (
            isinstance(self.min_confirmations, bool)
            or not isinstance(self.min_confirmations, int)
            or self.min_confirmations < 1
        ):
            raise ValueError("min_confirmations must be an integer >= 1")


@dataclass(frozen=True)
class TrackDiagnostics:
    """Per-frame animator counts."""

    smoothed_count: int
    fading_count: int
    active_tracks: int
    tentative_count: int = 0


class _Track:
    """Mutable per-object state (the E9 hot/cold cache)."""

    __slots__ = (
        "track_id",
        "class_id",
        "class_name",
        "smoothed_bbox",
        "last_polygon",
        "last_confidence",
        "consecutive",
        "frames_since_seen",
        "appearances",
    )

    def __init__(
        self,
        track_id: int,
        class_id: int,
        class_name: str,
        smoothed_bbox: Bbox,
        polygon: tuple[Point, ...],
        confidence: float,
    ) -> None:
        self.track_id = track_id
        self.class_id = class_id
        self.class_name = class_name
        self.smoothed_bbox = smoothed_bbox
        self.last_polygon = polygon
        self.last_confidence = confidence
        self.consecutive = 1
        self.frames_since_seen = 0
        self.appearances = 1

    def is_fading(self) -> bool:
        return self.frames_since_seen > 0


def _remap_polygon(
    polygon: Sequence[Point],
    source: Bbox,
    target: Bbox,
) -> tuple[Point, ...]:
    """Linearly map ``polygon`` from its own bounding box onto ``target``.

    Degenerate source axes (zero width/height) collapse onto the target
    axis mid so the transform stays well-defined.
    """
    sx0, sy0, sx1, sy1 = source
    tx0, ty0, tx1, ty1 = target
    sw = sx1 - sx0
    sh = sy1 - sy0
    sx_rate = (tx1 - tx0) / sw if sw > _EPSILON else 0.0
    sy_rate = (ty1 - ty0) / sh if sh > _EPSILON else 0.0
    tx_mid = (tx0 + tx1) / 2.0
    ty_mid = (ty0 + ty1) / 2.0
    mapped: list[Point] = []
    for x, y in polygon:
        nx = tx0 + (float(x) - sx0) * sx_rate if sw > _EPSILON else tx_mid
        ny = ty0 + (float(y) - sy0) * sy_rate if sh > _EPSILON else ty_mid
        mapped.append((nx, ny))
    return tuple(mapped)


def _scale_polygon(polygon: Sequence[Point], factor: float) -> tuple[Point, ...]:
    """Scale ``polygon`` toward its centroid by ``factor`` (1.0 = unchanged)."""
    cx, cy = polygon_centroid(polygon)
    return tuple((cx + (float(x) - cx) * factor, cy + (float(y) - cy) * factor) for x, y in polygon)


def _ease(t: float) -> float:
    """Ease-in-out quad on ``t`` clamped to [0, 1]."""
    t = min(1.0, max(0.0, t))
    if t < 0.5:
        return 2.0 * t * t
    return 1.0 - 2.0 * (1.0 - t) * (1.0 - t)


class TrackAnimator:
    """Persistent-track smoothing and in/out animation for detections."""

    def __init__(
        self,
        config: TrackSmoothingConfig | None = None,
        *,
        iou_threshold: float = DEFAULT_IOU_THRESHOLD,
        centroid_match_px: float = DEFAULT_CENTROID_MATCH_PX,
    ) -> None:
        self._config = config if config is not None else TrackSmoothingConfig()
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
        self._tracks: list[_Track] = []
        self._next_track_id = 1

    @property
    def config(self) -> TrackSmoothingConfig:
        return self._config

    def reset(self) -> None:
        """Drop every track (e.g. on a detected scene change)."""
        self._tracks.clear()

    def update(
        self,
        masks: Sequence[InstanceMask],
        *,
        frame_index: int,
    ) -> tuple[tuple[InstanceMask, ...], TrackDiagnostics]:
        """Associate, smooth, animate; return the masks to draw this frame.

        With ``enabled`` false this is an identity passthrough. Emitted masks
        keep the detector's ``confidence``/``class_id``/``class_name`` and
        carry the smoothed geometry with the current ``frame_index``.
        """
        current_masks = tuple(masks)
        if not self._config.enabled:
            return current_masks, TrackDiagnostics(
                smoothed_count=0, fading_count=0, active_tracks=0
            )

        config = self._config
        geometry = [
            (mask.class_id, polygon_centroid(mask.polygon), polygon_bbox(mask.polygon))
            for mask in current_masks
        ]
        mapping = associate_greedy(
            [
                (track.class_id, _bbox_centroid(track.smoothed_bbox), track.smoothed_bbox)
                for track in self._tracks
            ],
            geometry,
            iou_threshold=self._iou_threshold,
            centroid_match_px=self._centroid_match_px,
        )

        smoothing_factor = (
            1.0 if config.smoothing_rate <= 0.0 else 1.0 - math.exp(-config.smoothing_rate)
        )

        emitted: list[InstanceMask] = []
        matched_track_indices: set[int] = set()
        new_track_indices: set[int] = set()
        smoothed_count = 0

        for index, mask in enumerate(current_masks):
            class_id, _centroid, raw_bbox = geometry[index]
            should_emit = True
            if index in mapping:
                track = self._tracks[mapping[index]]
                matched_track_indices.add(mapping[index])
                # Hot-field update (E9): geometry every matched frame.
                track.smoothed_bbox = _lerp_bbox(track.smoothed_bbox, raw_bbox, smoothing_factor)
                track.last_polygon = mask.polygon
                track.last_confidence = mask.confidence
                track.consecutive += 1
                track.appearances += 1
                track.frames_since_seen = 0
                # Cold-field invalidation (E9): identity rewrite only on change.
                if track.class_id != class_id or track.class_name != mask.class_name:
                    track.class_id = class_id
                    track.class_name = mask.class_name
                polygon = _remap_polygon(mask.polygon, raw_bbox, track.smoothed_bbox)
                smoothed_count += 1
            else:
                track = _Track(
                    self._next_track_id,
                    class_id,
                    mask.class_name,
                    raw_bbox,
                    mask.polygon,
                    mask.confidence,
                )
                new_track_indices.add(len(self._tracks))
                self._next_track_id += 1
                self._tracks.append(track)
                # E71: hold the first frame(s) until the track is confirmed.
                if track.appearances < config.min_confirmations:
                    should_emit = False
                # Fade-in: grow from the centroid over the configured window.
                if config.fade_in_frames > 1:
                    factor = _ease(track.appearances / config.fade_in_frames)
                    factor = max(factor, 1.0 / config.fade_in_frames)
                    polygon = _scale_polygon(mask.polygon, factor)
                else:
                    polygon = mask.polygon

            if should_emit:
                emitted.append(
                    InstanceMask(
                        frame_index=frame_index,
                        polygon=polygon,
                        confidence=mask.confidence,
                        class_id=mask.class_id,
                        class_name=mask.class_name,
                    )
                )

        # Unmatched tracks: keep fading copies alive for the bounded window.
        fading_count = 0
        survivors: list[_Track] = []
        for track_index, track in enumerate(self._tracks):
            if track_index in matched_track_indices or track_index in new_track_indices:
                survivors.append(track)
                continue
            track.frames_since_seen += 1
            # A tentative track that vanishes was never drawn: drop it.
            if track.appearances < config.min_confirmations:
                continue
            if track.frames_since_seen > config.max_gap_frames:
                continue  # drop: identity memory exhausted
            if track.frames_since_seen <= config.fade_out_frames:
                progress = track.frames_since_seen / (config.fade_out_frames + 1)
                factor = 1.0 - _ease(progress)
                factor = max(factor, _MIN_FADE_FACTOR)
                emitted.append(
                    InstanceMask(
                        frame_index=frame_index,
                        polygon=_scale_polygon(track.last_polygon, factor),
                        # Confidence of the last real detection, decayed with
                        # the animation so consumers can tell fading geometry
                        # from a live detection.
                        confidence=track.last_confidence * factor,
                        class_id=track.class_id,
                        class_name=track.class_name,
                    )
                )
                fading_count += 1
            survivors.append(track)

        # Prune bound (E9): drop the least-recently-seen tracks first.
        if len(survivors) > config.max_tracks:
            survivors.sort(key=lambda t: (t.frames_since_seen, -t.track_id))
            survivors = survivors[: config.max_tracks]
        self._tracks = survivors

        return tuple(emitted), TrackDiagnostics(
            smoothed_count=smoothed_count,
            fading_count=fading_count,
            active_tracks=len(survivors),
            tentative_count=sum(
                1 for track in survivors if track.appearances < config.min_confirmations
            ),
        )


def _bbox_centroid(box: Bbox) -> tuple[float, float]:
    return (box[0] + box[2]) / 2.0, (box[1] + box[3]) / 2.0


def _lerp_bbox(current: Bbox, target: Bbox, factor: float) -> Bbox:
    """Move ``current`` toward ``target`` by ``factor`` (frame-rate independent)."""
    return (
        current[0] + (target[0] - current[0]) * factor,
        current[1] + (target[1] - current[1]) * factor,
        current[2] + (target[2] - current[2]) * factor,
        current[3] + (target[3] - current[3]) * factor,
    )
