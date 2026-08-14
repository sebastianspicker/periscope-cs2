"""OutlineRenderer, fill helpers, and polyline stroke dispatch."""

from __future__ import annotations

from collections.abc import Iterable

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.renderer.distance_draw import _draw_distance_outline
from cs2_vision_access.renderer.fills import apply_halo_fill, apply_tint_fill
from cs2_vision_access.renderer.jfa import _draw_jfa_outline
from cs2_vision_access.renderer.patterns import pattern_polyline_segments
from cs2_vision_access.renderer.roles import (
    RoleTreatment,
    TreatmentCatalog,
    draw_role_marker,
)
from cs2_vision_access.renderer.style import (
    DRAW_COORDINATE_SCALE,
    DRAW_COORDINATE_SHIFT,
    OutlineStyle,
    RenderDiagnostics,
    contrast_ratio,
    effective_dash_period,
    effective_instance_line_widths,
    effective_line_widths,
    parse_hex_bgr,
)


def current_frame_masks(
    predictions: Iterable[InstanceMask], frame_index: int
) -> tuple[InstanceMask, ...]:
    """Fail closed by discarding every mask not tied to the current frame."""
    return tuple(prediction for prediction in predictions if prediction.frame_index == frame_index)


class OutlineRenderer:
    def __init__(
        self,
        style: OutlineStyle | None = None,
        catalog: TreatmentCatalog | None = None,
    ) -> None:
        self.style = style or OutlineStyle()
        self.catalog = catalog

    def render(
        self,
        frame_bgr: np.ndarray,
        predictions: Iterable[InstanceMask],
        *,
        frame_index: int,
    ) -> np.ndarray:
        """Return a copy with current-frame visible contours rendered."""
        rendered, _diagnostics = self.render_with_diagnostics(
            frame_bgr,
            predictions,
            frame_index=frame_index,
        )
        return rendered

    def render_with_diagnostics(
        self,
        frame_bgr: np.ndarray,
        predictions: Iterable[InstanceMask],
        *,
        frame_index: int,
    ) -> tuple[np.ndarray, RenderDiagnostics]:
        """Render current masks and return the exact accepted/discarded counts."""
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
            raise ValueError("frame_bgr must have shape (height, width, 3)")
        try:
            import cv2
        except ImportError as error:  # pragma: no cover - dependency boundary
            raise RuntimeError(
                "OpenCV is required for rendering; install the project dependencies"
            ) from error

        if self.catalog is not None:
            return self._render_with_catalog(cv2, frame_bgr, predictions, frame_index=frame_index)
        return self._render_single_style(cv2, frame_bgr, predictions, frame_index=frame_index)

    def _render_single_style(
        self,
        cv2: object,
        frame_bgr: np.ndarray,
        predictions: Iterable[InstanceMask],
        *,
        frame_index: int,
    ) -> tuple[np.ndarray, RenderDiagnostics]:
        """Historical single-style path (pixel-preserving when catalog is None)."""
        output = frame_bgr.copy()
        height, width = output.shape[:2]
        base_inner, base_outer = effective_line_widths(self.style, height)
        dash_period = effective_dash_period(self.style, height)
        prediction_values = tuple(predictions)
        current_predictions = current_frame_masks(prediction_values, frame_index)
        stale_count = len(prediction_values) - len(current_predictions)
        # (fixed_contour, float_points_xy) for accepted instances
        accepted: list[tuple[np.ndarray, np.ndarray]] = []
        degenerate_count = 0
        for prediction in current_predictions:
            points = np.asarray(prediction.polygon, dtype=np.float32)
            if (
                points.ndim != 2
                or points.shape[0] < 3
                or points.shape[1:] != (2,)
                or not np.all(np.isfinite(points))
            ):
                degenerate_count += 1
                continue
            points[:, 0] = np.clip(points[:, 0], 0, width - 1)
            points[:, 1] = np.clip(points[:, 1], 0, height - 1)
            float_contour = points.reshape((-1, 1, 2))
            if abs(float(cv2.contourArea(float_contour))) <= 0:
                degenerate_count += 1
                continue
            fixed_contour = (
                np.rint(points * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
            )
            if len(np.unique(fixed_contour.reshape((-1, 2)), axis=0)) < 3:
                degenerate_count += 1
                continue
            if abs(float(cv2.contourArea(fixed_contour))) <= 0:
                degenerate_count += 1
                continue
            accepted.append((fixed_contour, points.copy()))

        diagnostics = RenderDiagnostics(
            predictions_received=len(prediction_values),
            current_predictions=len(current_predictions),
            stale_predictions_discarded=stale_count,
            degenerate_masks_discarded=degenerate_count,
            contours_rendered=len(accepted),
            inner_width_pixels=base_inner,
            outer_width_pixels=base_outer,
            stroke_contrast_ratio=contrast_ratio(
                self.style.inner_color,
                self.style.outer_color,
            ),
            stroke_pattern=self.style.stroke_pattern,
            dash_period_pixels=dash_period,
            roles_applied=None,
        )

        if not accepted:
            return output, diagnostics

        contours = [item[0] for item in accepted]
        if self.style.fill_opacity > 0:
            if self.style.fill_mode == "halo":
                output = apply_halo_fill(cv2, output, contours, self.style)
            else:
                output = apply_tint_fill(cv2, output, contours, self.style)

        use_batch_polyline = (
            self.style.outline_kernel == "polyline" and not self.style.adapt_width_to_area
        )
        if use_batch_polyline:
            # Historical parity path: one closed dual-stroke for all instances.
            draw_styled_polylines(
                cv2,
                output,
                contours,
                parse_hex_bgr(self.style.outer_color),
                base_outer,
                pattern=self.style.stroke_pattern,
                period_px=dash_period,
            )
            draw_styled_polylines(
                cv2,
                output,
                contours,
                parse_hex_bgr(self.style.inner_color),
                base_inner,
                pattern=self.style.stroke_pattern,
                period_px=dash_period,
            )
            return output, diagnostics

        outer_bgr = parse_hex_bgr(self.style.outer_color)
        inner_bgr = parse_hex_bgr(self.style.inner_color)
        for fixed_contour, float_points in accepted:
            inner_w, outer_w = effective_instance_line_widths(
                self.style, height, width, float_points
            )
            if self.style.outline_kernel == "distance":
                _draw_distance_outline(
                    cv2,
                    output,
                    fixed_contour,
                    outer_bgr,
                    inner_bgr,
                    outer_w,
                    inner_w,
                )
            elif self.style.outline_kernel == "jfa":
                _draw_jfa_outline(
                    cv2,
                    output,
                    fixed_contour,
                    outer_bgr,
                    inner_bgr,
                    outer_w,
                    inner_w,
                )
            else:
                draw_styled_polylines(
                    cv2,
                    output,
                    [fixed_contour],
                    outer_bgr,
                    outer_w,
                    pattern=self.style.stroke_pattern,
                    period_px=dash_period,
                )
                draw_styled_polylines(
                    cv2,
                    output,
                    [fixed_contour],
                    inner_bgr,
                    inner_w,
                    pattern=self.style.stroke_pattern,
                    period_px=dash_period,
                )
        return output, diagnostics

    def _render_with_catalog(
        self,
        cv2: object,
        frame_bgr: np.ndarray,
        predictions: Iterable[InstanceMask],
        *,
        frame_index: int,
    ) -> tuple[np.ndarray, RenderDiagnostics]:
        """Per-instance role treatments from a multi-signifier catalog."""
        assert self.catalog is not None
        catalog = self.catalog
        output = frame_bgr.copy()
        height, width = output.shape[:2]
        # Diagnostics report the catalog default style widths/pattern.
        base_style = catalog.default.style
        base_inner, base_outer = effective_line_widths(base_style, height)
        dash_period = effective_dash_period(base_style, height)
        prediction_values = tuple(predictions)
        current_predictions = current_frame_masks(prediction_values, frame_index)
        stale_count = len(prediction_values) - len(current_predictions)
        # (fixed_contour, float_points, treatment, role_key)
        accepted: list[tuple[np.ndarray, np.ndarray, RoleTreatment, str]] = []
        degenerate_count = 0
        for prediction in current_predictions:
            points = np.asarray(prediction.polygon, dtype=np.float32)
            if (
                points.ndim != 2
                or points.shape[0] < 3
                or points.shape[1:] != (2,)
                or not np.all(np.isfinite(points))
            ):
                degenerate_count += 1
                continue
            points[:, 0] = np.clip(points[:, 0], 0, width - 1)
            points[:, 1] = np.clip(points[:, 1], 0, height - 1)
            float_contour = points.reshape((-1, 1, 2))
            if abs(float(cv2.contourArea(float_contour))) <= 0:
                degenerate_count += 1
                continue
            fixed_contour = (
                np.rint(points * DRAW_COORDINATE_SCALE).astype(np.int32).reshape((-1, 1, 2))
            )
            if len(np.unique(fixed_contour.reshape((-1, 2)), axis=0)) < 3:
                degenerate_count += 1
                continue
            if abs(float(cv2.contourArea(fixed_contour))) <= 0:
                degenerate_count += 1
                continue
            treatment = catalog.resolve(prediction.class_name)
            role_key = catalog.resolve_key(prediction.class_name)
            accepted.append((fixed_contour, points.copy(), treatment, role_key))

        roles_counts: dict[str, int] = {}
        for _fixed, _pts, _treatment, role_key in accepted:
            roles_counts[role_key] = roles_counts.get(role_key, 0) + 1

        diagnostics = RenderDiagnostics(
            predictions_received=len(prediction_values),
            current_predictions=len(current_predictions),
            stale_predictions_discarded=stale_count,
            degenerate_masks_discarded=degenerate_count,
            contours_rendered=len(accepted),
            inner_width_pixels=base_inner,
            outer_width_pixels=base_outer,
            stroke_contrast_ratio=contrast_ratio(
                base_style.inner_color,
                base_style.outer_color,
            ),
            stroke_pattern=base_style.stroke_pattern,
            dash_period_pixels=dash_period,
            roles_applied=dict(sorted(roles_counts.items())),
        )

        if not accepted:
            return output, diagnostics

        # Per-instance fill
        for fixed_contour, _float_points, treatment, _role_key in accepted:
            style = treatment.style
            if style.fill_opacity > 0:
                if style.fill_mode == "halo":
                    output = apply_halo_fill(cv2, output, [fixed_contour], style)
                else:
                    output = apply_tint_fill(cv2, output, [fixed_contour], style)

        # Per-instance outline
        for fixed_contour, float_points, treatment, _role_key in accepted:
            style = treatment.style
            period = effective_dash_period(style, height)
            inner_w, outer_w = effective_instance_line_widths(style, height, width, float_points)
            outer_bgr = parse_hex_bgr(style.outer_color)
            inner_bgr = parse_hex_bgr(style.inner_color)
            if style.outline_kernel == "distance":
                _draw_distance_outline(
                    cv2, output, fixed_contour, outer_bgr, inner_bgr, outer_w, inner_w
                )
            elif style.outline_kernel == "jfa":
                _draw_jfa_outline(
                    cv2, output, fixed_contour, outer_bgr, inner_bgr, outer_w, inner_w
                )
            else:
                draw_styled_polylines(
                    cv2,
                    output,
                    [fixed_contour],
                    outer_bgr,
                    outer_w,
                    pattern=style.stroke_pattern,
                    period_px=period,
                )
                draw_styled_polylines(
                    cv2,
                    output,
                    [fixed_contour],
                    inner_bgr,
                    inner_w,
                    pattern=style.stroke_pattern,
                    period_px=period,
                )

        for _fixed_contour, float_points, treatment, _role_key in accepted:
            draw_role_marker(
                cv2,
                output,
                float_points,
                treatment,
                frame_height=height,
            )
        return output, diagnostics


def draw_styled_polylines(
    cv2: object,
    output: np.ndarray,
    contours: list[np.ndarray],
    color: tuple[int, int, int],
    thickness: int,
    *,
    pattern: str,
    period_px: int,
) -> None:
    """Draw dual-stroke polylines; solid keeps the closed-contour path.

    Contours are expected in fixed-point coordinates with
    ``shift=DRAW_COORDINATE_SHIFT`` (scale ``DRAW_COORDINATE_SCALE``).
    """
    polylines = cv2.polylines
    line_aa = cv2.LINE_AA
    if pattern == "solid":
        polylines(
            output,
            contours,
            True,
            color,
            thickness,
            lineType=line_aa,
            shift=DRAW_COORDINATE_SHIFT,
        )
        return

    segments: list[np.ndarray] = []
    for contour in contours:
        segments.extend(pattern_polyline_segments(contour, pattern, period_px))
    if not segments:
        return
    polylines(
        output,
        segments,
        False,
        color,
        thickness,
        lineType=line_aa,
        shift=DRAW_COORDINATE_SHIFT,
    )


# Back-compat alias for any remaining private callers.
_draw_styled_polylines = draw_styled_polylines
