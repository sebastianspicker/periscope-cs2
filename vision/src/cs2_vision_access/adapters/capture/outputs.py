"""Output sinks for the live pipeline — protocol and concrete implementations.

Output sinks receive each rendered frame and deliver it to an external
destination such as a video file, PNG sequence, or streaming compositor.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Protocol, runtime_checkable

import numpy as np


@runtime_checkable
class OutputSink(Protocol):
    """Protocol for outputting rendered frames to an external destination."""

    def send(self, frame: np.ndarray, frame_index: int) -> None:
        """Deliver a rendered frame to the sink.

        Args:
            frame: The rendered frame (BGR or RGBA uint8 array).
            frame_index: Zero-based frame index for sequencing.
        """
        ...

    def close(self) -> None:
        """Release any resources held by the sink."""


@dataclass
class FileOutputSink:
    """Write rendered frames to disk as a video or PNG image sequence.

    If ``output_path`` has a video extension (``.mp4``, ``.avi``, ``.mkv``)
    a video file is created. Otherwise frames are written as individual PNG
    images in a directory or as numbered files.

    Args:
        output_path: Destination file path or directory.
        fps: Target frame rate for video output (default 30.0).
    """

    output_path: str | Path
    fps: float = 30.0

    def __post_init__(self) -> None:
        self._writer: Any = None
        self._initialized = False

    def send(self, frame: np.ndarray, frame_index: int) -> None:
        """Write a rendered frame to the configured output destination."""
        import cv2

        path = Path(self.output_path)
        if path.suffix.lower() in (".mp4", ".avi", ".mkv"):
            if not self._initialized:
                path.parent.mkdir(parents=True, exist_ok=True)
                height, width = frame.shape[:2]
                fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                self._writer = cv2.VideoWriter(str(path), fourcc, self.fps, (width, height))
                self._initialized = True
            if self._writer:
                if frame.ndim == 3 and frame.shape[2] == 4:
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGBA2BGR)
                self._writer.write(frame)
            return

        # Image sequence:
        # - no suffix / directory path → <path>/frame_XXXXXX.png
        # - image suffix (e.g. .png) → <parent>/<stem>_XXXXXX<suffix>
        #   so multi-frame live dumps do not overwrite a single file.
        image_suffixes = {".png", ".jpg", ".jpeg", ".bmp", ".webp"}
        if not path.suffix:
            output = path / f"frame_{frame_index:06d}.png"
        elif path.suffix.lower() in image_suffixes:
            output = path.parent / f"{path.stem}_{frame_index:06d}{path.suffix}"
        else:
            output = path.parent / f"{path.stem}_{frame_index:06d}.png"
        output.parent.mkdir(parents=True, exist_ok=True)
        cv2.imwrite(str(output), frame)

    def close(self) -> None:
        """Release the video writer when one was opened."""
        if self._writer:
            self._writer.release()
