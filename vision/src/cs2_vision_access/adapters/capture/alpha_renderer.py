"""Alpha-only outline renderer — outputs RGBA frames with transparent background.

Use cases:
  - OBS / KM Box / streaming compositing: output just the outlines with alpha
  - Color-key compositing: render on green background for hardware chroma-keying
  - Side-by-side: overlay view + alpha view for comparison

Two output modes:
  ``alpha`` — RGBA with transparent background, just the outlines visible
  ``green`` — RGB with green (#00FF00) background for chroma-key compositing
  ``overlay`` — Standard BGR overlay on the captured frame (like the file pipeline)
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass

import numpy as np

# OpenCV is a hard dependency; keep the name on the module so tests can patch it.
try:
    import cv2
except ImportError:  # pragma: no cover
    cv2 = None  # type: ignore[assignment]

from cs2_vision_access.adapters.rendering.renderer import (
    OutlineRenderer,
    OutlineStyle,
    current_frame_masks,
    effective_dash_period,
    effective_instance_line_widths,
    parse_hex_bgr,
)
from cs2_vision_access.adapters.rendering.renderer.distance_draw import _draw_distance_outline
from cs2_vision_access.adapters.rendering.renderer.draw import draw_styled_polylines
from cs2_vision_access.adapters.rendering.renderer.jfa import _draw_jfa_outline
from cs2_vision_access.adapters.rendering.renderer.roles import (
    TreatmentCatalog,
    draw_role_marker,
)
from cs2_vision_access.domain.outline import DRAW_COORDINATE_SCALE
from cs2_vision_access.domain.predictions import InstanceMask

ALPHA_OUTPUT_MODES = frozenset({"alpha", "green", "overlay"})


@dataclass(frozen=True)
class AlphaRendererConfig:
    """Configuration for the alpha/green-screen renderer.

    ``output_mode``:
      - ``alpha`` — RGBA output with transparent background
      - ``green`` — BGR output with green background (chroma key)
      - ``overlay`` — Standard overlay on the captured frame

    ``fill_alpha`` — When in alpha mode, the fill opacity is scaled by this
    factor so interior tint is semi-transparent on the alpha channel too.

    ``chroma_key_color`` — BGR tuple for the green-screen background color.
    """

    output_mode: str = "alpha"
    enable_fill: bool = False  # Disable interior fill in alpha mode by default (cleaner)
    chroma_key_color: tuple[int, int, int] = (0, 255, 0)  # BGR green


class AlphaRenderer:
    """Renders outlines onto a transparent or green-screen background.

    This is a companion to ``OutlineRenderer`` that produces output suitable
    for compositing pipelines (OBS, KM Box, streaming software).

    The renderer accepts the same ``OutlineStyle`` and ``TreatmentCatalog``
    for visual consistency with the offline pipeline.
    """

    def __init__(
        self,
        style: OutlineStyle | None = None,
        catalog: TreatmentCatalog | None = None,
        alpha_config: AlphaRendererConfig | None = None,
    ) -> None:
        self.style = style or OutlineStyle()
        self.catalog = catalog
        self.alpha_config = alpha_config or AlphaRendererConfig()
        self._inner_renderer = OutlineRenderer(style, catalog=catalog)

    def render(
        self,
        frame_bgr: np.ndarray | None,
        predictions: Iterable[InstanceMask],
        *,
        frame_index: int,
    ) -> np.ndarray:
        """Return an RGBA or BGR frame with outlines rendered.

        Args:
            frame_bgr: The captured BGR frame. May be None in alpha/green mode
                (the output size is determined from predictions or a default).
            predictions: InstanceMask predictions for this frame.
            frame_index: The zero-based frame index.

        Returns:
            Either an RGBA uint8 array (alpha mode) or BGR uint8 array (green/overlay mode).
        """
        output_mode = self.alpha_config.output_mode
        # CHANGED: Reject unsupported modes instead of falling back to green.
        if output_mode not in ALPHA_OUTPUT_MODES:
            valid_modes = ", ".join(sorted(ALPHA_OUTPUT_MODES))
            raise ValueError(
                f"Unsupported output mode {output_mode!r}; expected one of: {valid_modes}"
            )

        if output_mode == "overlay":
            # Delegate to the standard OutlineRenderer
            return self._inner_renderer.render(
                frame_bgr if frame_bgr is not None else np.zeros((720, 1280, 3), dtype=np.uint8),
                predictions,
                frame_index=frame_index,
            )

        # For alpha and green modes: determine output dimensions
        if frame_bgr is not None:
            height, width = frame_bgr.shape[:2]
        else:
            height, width = 720, 1280  # fallback

        if output_mode == "alpha":
            # RGBA with transparent background
            output = np.zeros((height, width, 4), dtype=np.uint8)
        else:
            # Green screen background
            output = np.full((height, width, 3), self.alpha_config.chroma_key_color, dtype=np.uint8)

        if cv2 is None:
            raise RuntimeError("OpenCV is required for rendering; install the project dependencies")

        # Filter to current frame only
        prediction_values = tuple(predictions)
        current = current_frame_masks(prediction_values, frame_index)
        if not current:
            return output

        # Draw outlines on a temporary BGR buffer, then composite
        bgr_buffer = np.zeros((height, width, 3), dtype=np.uint8)

        for prediction in current:
            points = np.asarray(prediction.polygon, dtype=np.float32)
            if (
                points.ndim != 2
                or points.shape[0] < 3
                or points.shape[1:] != (2,)
                or not np.all(np.isfinite(points))
            ):
                continue

            points[:, 0] = np.clip(points[:, 0], 0, width - 1)
            points[:, 1] = np.clip(points[:, 1], 0, height - 1)

            # Use fixed-point contour for drawing (shift=DRAW_COORDINATE_SHIFT)
            fixed = np.rint(points * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
            if abs(float(cv2.contourArea(fixed))) <= 0:
                continue

            inner_w, outer_w = effective_instance_line_widths(self.style, height, width, points)
            outer_bgr = parse_hex_bgr(self.style.outer_color)
            inner_bgr = parse_hex_bgr(self.style.inner_color)

            # Draw dual-stroke outlines on the BGR buffer
            kernel = self.style.outline_kernel
            if kernel == "distance":
                _draw_distance_outline(
                    cv2,
                    bgr_buffer,
                    fixed,
                    outer_bgr,
                    inner_bgr,
                    outer_w,
                    inner_w,
                )
            elif kernel == "jfa":
                _draw_jfa_outline(
                    cv2,
                    bgr_buffer,
                    fixed,
                    outer_bgr,
                    inner_bgr,
                    outer_w,
                    inner_w,
                )
            else:
                draw_styled_polylines(
                    cv2,
                    bgr_buffer,
                    [fixed],
                    outer_bgr,
                    outer_w,
                    pattern=self.style.stroke_pattern,
                    period_px=effective_dash_period(self.style, height),
                )
                draw_styled_polylines(
                    cv2,
                    bgr_buffer,
                    [fixed],
                    inner_bgr,
                    inner_w,
                    pattern=self.style.stroke_pattern,
                    period_px=effective_dash_period(self.style, height),
                )

            # Role marker
            if self.catalog is not None:
                treatment = self.catalog.resolve(prediction.class_name)
                draw_role_marker(cv2, bgr_buffer, points, treatment, frame_height=height)

        if output_mode == "alpha":
            # CHANGED: Build a thresholded coverage mask with smoother alpha edges.
            gray = cv2.cvtColor(bgr_buffer, cv2.COLOR_BGR2GRAY)
            _alpha = np.where(gray >= 8, 255, 0).astype(np.uint8)
            _alpha = cv2.GaussianBlur(_alpha, (5, 5), 0)

            # CHANGED: RGBA is RGB-ordered, while drawing uses OpenCV's BGR order.
            output[:, :, :3] = cv2.cvtColor(bgr_buffer, cv2.COLOR_BGR2RGB)
            output[:, :, 3] = _alpha

            # CHANGED: Composite optional fill source-over the outline RGBA pixels.
            if self.alpha_config.enable_fill and self.style.fill_opacity > 0:
                fill_mask = np.zeros((height, width), dtype=np.uint8)
                for prediction in current:
                    pts = np.asarray(prediction.polygon, dtype=np.int32).reshape((-1, 1, 2))
                    cv2.fillPoly(
                        fill_mask,
                        [pts],
                        255,
                    )

                source_alpha = fill_mask.astype(np.float32) / 255.0 * self.style.fill_opacity
                destination_alpha = output[:, :, 3].astype(np.float32) / 255.0
                output_alpha = source_alpha + destination_alpha * (1.0 - source_alpha)

                fill_rgb = np.asarray(parse_hex_bgr(self.style.inner_color)[::-1], dtype=np.float32)
                destination_rgb = output[:, :, :3].astype(np.float32)
                premultiplied_rgb = fill_rgb * source_alpha[
                    :, :, np.newaxis
                ] + destination_rgb * destination_alpha[:, :, np.newaxis] * (
                    1.0 - source_alpha[:, :, np.newaxis]
                )
                composited_rgb = np.zeros_like(premultiplied_rgb)
                np.divide(
                    premultiplied_rgb,
                    output_alpha[:, :, np.newaxis],
                    out=composited_rgb,
                    where=output_alpha[:, :, np.newaxis] > 0,
                )
                output[:, :, :3] = np.rint(composited_rgb).astype(np.uint8)
                output[:, :, 3] = np.rint(output_alpha * 255.0).astype(np.uint8)
        else:
            # Green mode: overlay outlines on green background
            mask = cv2.cvtColor(bgr_buffer, cv2.COLOR_BGR2GRAY) > 0
            output[mask] = bgr_buffer[mask]

        return output
