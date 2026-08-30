"""OutlinePreferences dataclass: resolve and from_style."""

from __future__ import annotations

from dataclasses import dataclass

from cs2_vision_access.application.configuration.prefs.coerce import (
    _optional_bool,
    _optional_color,
    _optional_fill_mode,
    _optional_float,
    _optional_int,
    _optional_outline_kernel,
    _optional_stroke_pattern,
    _required_text,
)
from cs2_vision_access.application.configuration.prefs.errors import OutlinePreferencesError
from cs2_vision_access.domain.outline import OutlineStyle, get_outline_preset

SCHEMA_VERSION = 1
DEFAULT_PRESET = "high-visibility"


@dataclass(frozen=True)
class OutlinePreferences:
    """Preset name plus optional overrides that resolve to an ``OutlineStyle``."""

    schema_version: int = SCHEMA_VERSION
    preset: str = DEFAULT_PRESET
    inner_color: str | None = None
    outer_color: str | None = None
    inner_width: int | None = None
    outer_width: int | None = None
    fill_opacity: float | None = None
    scale_with_frame: bool | None = None
    stroke_pattern: str | None = None
    dash_period_px: int | None = None
    fill_mode: str | None = None
    halo_blur: int | None = None
    adapt_width_to_area: bool | None = None
    outline_kernel: str | None = None

    def __post_init__(self) -> None:
        if (
            isinstance(self.schema_version, bool)
            or not isinstance(self.schema_version, int)
            or self.schema_version != SCHEMA_VERSION
        ):
            raise OutlinePreferencesError(f"schema_version must be {SCHEMA_VERSION}")
        # Normalize and type-check the same way as the JSON load path so that
        # anything constructed (and later saved) is always reloadable.
        object.__setattr__(self, "preset", _required_text(self.preset, "preset"))
        object.__setattr__(self, "inner_color", _optional_color(self.inner_color, "inner_color"))
        object.__setattr__(self, "outer_color", _optional_color(self.outer_color, "outer_color"))
        object.__setattr__(self, "inner_width", _optional_int(self.inner_width, "inner_width"))
        object.__setattr__(self, "outer_width", _optional_int(self.outer_width, "outer_width"))
        object.__setattr__(self, "fill_opacity", _optional_float(self.fill_opacity, "fill_opacity"))
        object.__setattr__(
            self,
            "scale_with_frame",
            _optional_bool(self.scale_with_frame, "scale_with_frame"),
        )
        object.__setattr__(
            self,
            "stroke_pattern",
            _optional_stroke_pattern(self.stroke_pattern, "stroke_pattern"),
        )
        object.__setattr__(
            self,
            "dash_period_px",
            _optional_int(self.dash_period_px, "dash_period_px"),
        )
        object.__setattr__(self, "fill_mode", _optional_fill_mode(self.fill_mode, "fill_mode"))
        object.__setattr__(self, "halo_blur", _optional_int(self.halo_blur, "halo_blur"))
        object.__setattr__(
            self,
            "adapt_width_to_area",
            _optional_bool(self.adapt_width_to_area, "adapt_width_to_area"),
        )
        object.__setattr__(
            self,
            "outline_kernel",
            _optional_outline_kernel(self.outline_kernel, "outline_kernel"),
        )
        # Fail closed immediately: unknown preset or invalid overrides.
        self.resolve()

    def resolve(self) -> OutlineStyle:
        """Apply overrides on top of the named preset; validates via OutlineStyle."""
        try:
            base = get_outline_preset(self.preset)
        except ValueError as error:
            raise OutlinePreferencesError(str(error)) from error
        try:
            return OutlineStyle(
                inner_color=(base.inner_color if self.inner_color is None else self.inner_color),
                outer_color=(base.outer_color if self.outer_color is None else self.outer_color),
                inner_width=(base.inner_width if self.inner_width is None else self.inner_width),
                outer_width=(base.outer_width if self.outer_width is None else self.outer_width),
                fill_opacity=(
                    base.fill_opacity if self.fill_opacity is None else self.fill_opacity
                ),
                scale_with_frame=(
                    base.scale_with_frame
                    if self.scale_with_frame is None
                    else self.scale_with_frame
                ),
                stroke_pattern=(
                    base.stroke_pattern if self.stroke_pattern is None else self.stroke_pattern
                ),
                dash_period_px=(
                    base.dash_period_px if self.dash_period_px is None else self.dash_period_px
                ),
                fill_mode=(base.fill_mode if self.fill_mode is None else self.fill_mode),
                halo_blur=(base.halo_blur if self.halo_blur is None else self.halo_blur),
                adapt_width_to_area=(
                    base.adapt_width_to_area
                    if self.adapt_width_to_area is None
                    else self.adapt_width_to_area
                ),
                outline_kernel=(
                    base.outline_kernel if self.outline_kernel is None else self.outline_kernel
                ),
            )
        except ValueError as error:
            raise OutlinePreferencesError(str(error)) from error

    def as_json(self) -> dict[str, object]:
        """Serialize to a deterministic JSON-ready mapping (null = no override)."""
        return {
            "schema_version": self.schema_version,
            "preset": self.preset,
            "inner_color": self.inner_color,
            "outer_color": self.outer_color,
            "inner_width": self.inner_width,
            "outer_width": self.outer_width,
            "fill_opacity": self.fill_opacity,
            "scale_with_frame": self.scale_with_frame,
            "stroke_pattern": self.stroke_pattern,
            "dash_period_px": self.dash_period_px,
            "fill_mode": self.fill_mode,
            "halo_blur": self.halo_blur,
            "adapt_width_to_area": self.adapt_width_to_area,
            "outline_kernel": self.outline_kernel,
        }

    @classmethod
    def from_style(
        cls,
        style: OutlineStyle,
        *,
        preset: str = DEFAULT_PRESET,
    ) -> OutlinePreferences:
        """Build preferences that fully override ``preset`` to match ``style``."""
        return cls(
            schema_version=SCHEMA_VERSION,
            preset=preset,
            inner_color=style.inner_color,
            outer_color=style.outer_color,
            inner_width=style.inner_width,
            outer_width=style.outer_width,
            fill_opacity=style.fill_opacity,
            scale_with_frame=style.scale_with_frame,
            stroke_pattern=style.stroke_pattern,
            dash_period_px=style.dash_period_px,
            fill_mode=style.fill_mode,
            halo_blur=style.halo_blur,
            adapt_width_to_area=style.adapt_width_to_area,
            outline_kernel=style.outline_kernel,
        )


def default_outline_preferences() -> OutlinePreferences:
    """Return preferences equal to the current high-visibility outline style."""
    return OutlinePreferences()
