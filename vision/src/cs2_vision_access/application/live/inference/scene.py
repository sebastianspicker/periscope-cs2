"""Scene-change detection for live pipeline state resets (ledger item E31).

Re-initializing expensive per-scene context only when a cheap identity signal
changes. Here the "context" is the temporal filter's and track animator's
association history: when the scene cuts (map change, respawn camera, menu),
all tracks and stability history describe objects that no longer exist, so
carrying them across the cut produces stale, wrong outlines.

The detector is deliberately cheap and dependency-free: it compares a
downsampled grayscale thumbnail of the current frame against the previous
one (numpy slicing only, no OpenCV in the application layer) and reports a
cut when the mean absolute difference exceeds ``threshold`` (0-255 scale),
subject to a ``cooldown_frames`` minimum spacing between reported cuts.

Opt-in via ``SceneChangeConfig.enabled`` (default false), like the rest of
the live pipeline's behaviours.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

_THUMBNAIL_MAX_DIM = 64


@dataclass(frozen=True)
class SceneChangeConfig:
    """Configuration for scene-change detection.

    ``threshold`` is the mean absolute per-pixel difference (0-255) above
    which a frame boundary counts as a scene cut. ``cooldown_frames`` is the
    minimum number of updates between two reported cuts (debounce). The
    first update after construction/reset never reports a cut (there is no
    previous frame to differ from).
    """

    enabled: bool = False
    threshold: float = 18.0
    cooldown_frames: int = 5

    def __post_init__(self) -> None:
        if (
            isinstance(self.threshold, bool)
            or not isinstance(self.threshold, (int, float))
            or not 0.0 <= float(self.threshold) <= 255.0
        ):
            raise ValueError("threshold must be a number in [0, 255]")
        if (
            isinstance(self.cooldown_frames, bool)
            or not isinstance(self.cooldown_frames, int)
            or self.cooldown_frames < 0
        ):
            raise ValueError("cooldown_frames must be a non-negative integer")


class SceneChangeDetector:
    """Cheap frame-difference scene-cut detector (no OpenCV dependency)."""

    def __init__(self, config: SceneChangeConfig | None = None) -> None:
        self._config = config if config is not None else SceneChangeConfig()
        self._previous: np.ndarray | None = None
        # Frames since the last reported cut; start at the cooldown so the
        # first detected cut is reported immediately rather than debounced.
        self._frames_since_cut = self._config.cooldown_frames

    @property
    def config(self) -> SceneChangeConfig:
        return self._config

    def reset(self) -> None:
        """Forget the previous thumbnail (next update cannot report a cut)."""
        self._previous = None
        self._frames_since_cut = self._config.cooldown_frames

    def update(self, frame_bgr: np.ndarray) -> bool:
        """Feed one captured frame; return True when a scene cut is detected.

        ``frame_bgr`` is the captured frame in the pipeline's native BGR
        layout; only shape and a decimated sample are read.
        """
        if not self._config.enabled:
            return False
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] < 1:
            return False

        thumbnail = _thumbnail(frame_bgr)
        previous = self._previous
        self._previous = thumbnail
        if previous is None:
            return False
        if previous.shape != thumbnail.shape:
            # Resolution changed (resize, source switch): treat as a cut.
            self._frames_since_cut = 0
            return True

        difference = np.abs(thumbnail.astype(np.int16) - previous.astype(np.int16))
        mean_difference = float(difference.mean())
        if mean_difference <= self._config.threshold:
            self._frames_since_cut += 1
            return False
        if self._frames_since_cut < self._config.cooldown_frames:
            self._frames_since_cut += 1
            return False
        self._frames_since_cut = 0
        return True


def _thumbnail(frame_bgr: np.ndarray) -> np.ndarray:
    """Decimated single-channel thumbnail (numpy only, no OpenCV)."""
    height, width = frame_bgr.shape[:2]
    step_y = max(1, height // _THUMBNAIL_MAX_DIM)
    step_x = max(1, width // _THUMBNAIL_MAX_DIM)
    decimated = frame_bgr[::step_y, ::step_x, :3]
    # Perceptual-ish luma weights over BGR order.
    weights = np.asarray([0.114, 0.587, 0.299], dtype=np.float32)
    return (decimated.astype(np.float32) @ weights).astype(np.uint8)
