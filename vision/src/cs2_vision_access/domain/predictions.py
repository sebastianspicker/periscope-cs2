"""Backend-neutral prediction value objects."""

from __future__ import annotations

from dataclasses import dataclass

Point = tuple[float, float]


@dataclass(frozen=True)
class InstanceMask:
    """One visible object contour tied to the frame that produced it."""

    frame_index: int
    polygon: tuple[Point, ...]
    confidence: float
    class_id: int
    class_name: str

    def __post_init__(self) -> None:
        if self.frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if len(self.polygon) < 3:
            raise ValueError("polygon must contain at least three points")
        if not 0.0 <= self.confidence <= 1.0:
            raise ValueError("confidence must be in [0, 1]")
        if self.class_id < 0:
            raise ValueError("class_id must be non-negative")
        if not self.class_name.strip():
            raise ValueError("class_name must not be empty")
