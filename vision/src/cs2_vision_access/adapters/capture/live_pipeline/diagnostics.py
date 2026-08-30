"""Diagnostics accumulator and timing helpers for the live pipeline."""

from __future__ import annotations

from dataclasses import dataclass, field

# Re-export the neutral shared linear-interpolation percentile.
from cs2_vision_access.domain.statistics import percentile as _percentile

__all__ = ["_Diagnostics", "_percentile"]


@dataclass
class _Diagnostics:
    inference_ms: list[float] = field(default_factory=list)
    pipeline_ms: list[float] = field(default_factory=list)
    _last_print: float = 0.0
    _accumulated_pause: float = 0.0

    def maybe_print(
        self,
        now: float,
        interval: float,
        frame_index: int,
        started: float,
        frames_behind: int,
        instances_predicted: int,
        instances_outlined: int,
    ) -> None:
        elapsed = now - started - self._accumulated_pause
        if now - self._last_print >= interval:
            effective_fps = frame_index / elapsed if elapsed > 0 else 0.0
            avg_inference = (
                sum(self.inference_ms[-100:]) / len(self.inference_ms[-100:])
                if self.inference_ms
                else 0.0
            )
            print(
                f"[live] frame={frame_index} fps={effective_fps:.1f} "
                f"infer={avg_inference:.1f}ms behind={frames_behind} "
                f"pred={instances_predicted} outlined={instances_outlined}"
            )
            self._last_print = now

    def mark_pause(self, duration: float) -> None:
        self._accumulated_pause += duration
