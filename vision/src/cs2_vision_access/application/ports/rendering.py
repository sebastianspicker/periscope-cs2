"""Backend-neutral rendering contracts used by application and workflows."""

from __future__ import annotations

from typing import Any, Protocol

from cs2_vision_access.domain.outline import OutlineStyle, RenderDiagnostics
from cs2_vision_access.domain.predictions import InstanceMask


class OutlineRenderer(Protocol):
    """Renders current-frame masks with a validated immutable style."""

    style: OutlineStyle

    def render(
        self, frame: Any, predictions: tuple[InstanceMask, ...], *, frame_index: int
    ) -> Any: ...

    def render_with_diagnostics(
        self, frame: Any, predictions: tuple[InstanceMask, ...], *, frame_index: int
    ) -> tuple[Any, RenderDiagnostics]: ...


class CueLogWriter(Protocol):
    """Append-only cue persistence boundary for offline processing."""

    @property
    def path(self) -> Any: ...

    @property
    def events_written(self) -> int: ...

    def observe(self, frame_index: int, masks: tuple[InstanceMask, ...]) -> None: ...

    def close(self, *, commit: bool = True) -> None: ...
