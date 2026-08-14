"""Shared fakes for video loop contract tests."""

from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace

import numpy as np

from cs2_vision_access.renderer import OutlineStyle, RenderDiagnostics


class _Capture:
    def __init__(
        self,
        frames: list[np.ndarray],
        events: list[str],
        *,
        opened: bool = True,
    ) -> None:
        self.frames = frames
        self.events = events
        self.opened = opened
        self.cursor = 0
        self.released = False

    def isOpened(self) -> bool:
        return self.opened

    def get(self, property_id: int) -> float:
        return {1: 30.0, 2: 16.0, 3: 12.0}[property_id]

    def read(self) -> tuple[bool, np.ndarray | None]:
        if self.cursor == len(self.frames):
            return False, None
        self.events.append(f"decode:{self.cursor}")
        frame = self.frames[self.cursor]
        self.cursor += 1
        return True, frame

    def release(self) -> None:
        self.released = True


class _Segmenter:
    def __init__(
        self,
        events: list[str],
        *,
        fail: bool = False,
        backend: str | None = None,
    ) -> None:
        self.events = events
        self.fail = fail
        if backend is not None:
            self.backend = backend

    def predict(self, _frame: np.ndarray, *, frame_index: int) -> tuple[object, ...]:
        self.events.append(f"infer:{frame_index}")
        if self.fail:
            raise RuntimeError("inference fixture failed")
        return (object(),)


class _Writer:
    def __init__(self, path: str, dimensions: tuple[int, int]) -> None:
        self.path = Path(path)
        self.dimensions = dimensions
        self.frames: list[tuple[int, ...]] = []
        self.released = False
        self.path.write_bytes(b"partial fixture")

    def isOpened(self) -> bool:
        return True

    def write(self, frame: np.ndarray) -> None:
        self.frames.append(frame.shape)

    def release(self) -> None:
        self.released = True


class _Renderer:
    def __init__(self, events: list[str]) -> None:
        self.events = events
        self.style = OutlineStyle()

    def render_with_diagnostics(
        self,
        frame: np.ndarray,
        _predictions: tuple[object, ...],
        *,
        frame_index: int,
    ) -> tuple[np.ndarray, RenderDiagnostics]:
        self.events.append(f"render:{frame_index}")
        return (
            frame.copy(),
            RenderDiagnostics(
                predictions_received=1,
                current_predictions=1,
                stale_predictions_discarded=0,
                degenerate_masks_discarded=0,
                contours_rendered=1,
                inner_width_pixels=3,
                outer_width_pixels=7,
                stroke_contrast_ratio=10.0,
                stroke_pattern="solid",
                dash_period_pixels=12,
            ),
        )


def _fake_cv2(capture: _Capture) -> object:
    return SimpleNamespace(
        CAP_PROP_FPS=1,
        CAP_PROP_FRAME_WIDTH=2,
        CAP_PROP_FRAME_HEIGHT=3,
        VideoCapture=lambda _path: capture,
    )
