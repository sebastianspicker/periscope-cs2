"""Shared fixtures for self-train / active-learning tests."""

from __future__ import annotations

from cs2_vision_access.predictions import InstanceMask


def _mask(
    polygon: tuple[tuple[float, float], ...],
    *,
    confidence: float = 0.9,
    class_id: int = 0,
    class_name: str = "ct",
    frame_index: int = 0,
) -> InstanceMask:
    return InstanceMask(
        frame_index=frame_index,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=class_name,
    )


SQUARE = ((10.0, 10.0), (40.0, 10.0), (40.0, 40.0), (10.0, 40.0))
SQUARE_NEAR = ((12.0, 11.0), (42.0, 11.0), (42.0, 41.0), (12.0, 41.0))
OTHER = ((100.0, 100.0), (130.0, 100.0), (130.0, 130.0), (100.0, 130.0))
TINY_BLIP = ((200.0, 200.0), (210.0, 200.0), (210.0, 210.0), (200.0, 210.0))
