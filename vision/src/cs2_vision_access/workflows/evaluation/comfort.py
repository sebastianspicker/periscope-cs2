"""Comfort proxies (clutter + local stroke-vs-background contrast)."""

from __future__ import annotations

import json
import math
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from cs2_vision_access.domain.outline import parse_hex_bgr
from cs2_vision_access.domain.predictions import Point
from cs2_vision_access.workflows.evaluation.errors import EvaluationError
from cs2_vision_access.workflows.evaluation.frames import _parse_prediction_instance
from cs2_vision_access.workflows.evaluation.geometry import (
    SCHEMA_VERSION,
    PolygonInstance,
    _json_float,
    rasterize_polygon,
)
from cs2_vision_access.workflows.evaluation.image_io import _load_frame_bgr
from cs2_vision_access.workflows.evaluation.luminance import (
    _bgr_relative_luminance,
    _hex_relative_luminance,
    _luminance_contrast_ratio,
)

DEFAULT_STROKE_CONTRAST_THRESHOLD = 3.0
DEFAULT_CONTRAST_SAMPLE_SPACING_PX = 4.0
DEFAULT_OUTWARD_OFFSET_PX = 3.0
DEFAULT_BACKGROUND_RADIUS_PX = 1


@dataclass(frozen=True)
class ComfortEvaluationResult:
    """Non-subjective comfort *proxies* for outlined frames (not user ratings)."""

    schema_version: int
    frame_width: int
    frame_height: int
    prediction_count: int
    clutter_fraction: float
    local_stroke_contrast_ge_3_fraction: float
    contrast_sample_count: int
    outer_color: str
    inner_color: str
    contrast_threshold: float

    def as_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "frame_width": self.frame_width,
            "frame_height": self.frame_height,
            "prediction_count": self.prediction_count,
            "clutter_fraction": _json_float(self.clutter_fraction),
            "local_stroke_contrast_ge_3_fraction": _json_float(
                self.local_stroke_contrast_ge_3_fraction
            ),
            "contrast_sample_count": self.contrast_sample_count,
            "outer_color": self.outer_color,
            "inner_color": self.inner_color,
            "contrast_threshold": _json_float(self.contrast_threshold),
        }


def clutter_fraction(
    polygons: Sequence[Sequence[Point]],
    *,
    height: int,
    width: int,
) -> float:
    """Fraction of frame pixels covered by the union of rasterized polygons."""
    if height <= 0 or width <= 0:
        raise EvaluationError("height and width must be positive")
    if not polygons:
        return 0.0
    union = np.zeros((height, width), dtype=bool)
    for polygon in polygons:
        mask = rasterize_polygon(polygon, height=height, width=width)
        union |= mask
    return float(union.sum()) / float(height * width)


def local_stroke_contrast_samples(
    frame_bgr: np.ndarray,
    polygons: Sequence[Sequence[Point]],
    *,
    outer_color: str,
    sample_spacing_px: float = DEFAULT_CONTRAST_SAMPLE_SPACING_PX,
    outward_offset_px: float = DEFAULT_OUTWARD_OFFSET_PX,
    background_radius_px: int = DEFAULT_BACKGROUND_RADIUS_PX,
) -> tuple[float, ...]:
    """WCAG contrast ratios between outer stroke color and local background.

    Samples polygon edges at ``sample_spacing_px`` intervals, steps
    ``outward_offset_px`` along the outward normal, and averages background
    luminance in a ``background_radius_px`` square. Returns one ratio per
    valid sample (skips samples that leave the frame).
    """
    if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
        raise EvaluationError("frame must be an HxWx3 BGR array")
    height, width = frame_bgr.shape[:2]
    if height <= 0 or width <= 0:
        raise EvaluationError("frame dimensions must be positive")
    if sample_spacing_px <= 0 or not math.isfinite(sample_spacing_px):
        raise EvaluationError("sample_spacing_px must be positive and finite")
    if outward_offset_px <= 0 or not math.isfinite(outward_offset_px):
        raise EvaluationError("outward_offset_px must be positive and finite")
    if background_radius_px < 0:
        raise EvaluationError("background_radius_px must be non-negative")

    outer_luminance = _hex_relative_luminance(outer_color)
    ratios: list[float] = []
    for polygon in polygons:
        for sample_x, sample_y, normal_x, normal_y in _edge_samples(
            polygon,
            spacing_px=sample_spacing_px,
        ):
            bg_x = sample_x + normal_x * outward_offset_px
            bg_y = sample_y + normal_y * outward_offset_px
            bg_lum = _sample_background_luminance(
                frame_bgr,
                bg_x,
                bg_y,
                radius=background_radius_px,
            )
            if bg_lum is None:
                continue
            ratios.append(_luminance_contrast_ratio(outer_luminance, bg_lum))
    return tuple(ratios)


def evaluate_comfort(
    frame_bgr: np.ndarray,
    predictions: Sequence[PolygonInstance],
    *,
    outer_color: str = "#101010",
    inner_color: str = "#F6FF00",
    contrast_threshold: float = DEFAULT_STROKE_CONTRAST_THRESHOLD,
    sample_spacing_px: float = DEFAULT_CONTRAST_SAMPLE_SPACING_PX,
    outward_offset_px: float = DEFAULT_OUTWARD_OFFSET_PX,
    background_radius_px: int = DEFAULT_BACKGROUND_RADIUS_PX,
) -> ComfortEvaluationResult:
    """Compute clutter fraction and local outer-stroke contrast pass rate.

    Proxies only — not a substitute for low-vision user ratings. No acceptance
    threshold is asserted beyond the reported ``contrast_threshold`` used for
    the ``local_stroke_contrast_ge_3_fraction`` numerator (default 3.0, WCAG
    non-text engineering target documented in RESEARCH).
    """
    if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
        raise EvaluationError("frame must be an HxWx3 BGR array")
    height, width = int(frame_bgr.shape[0]), int(frame_bgr.shape[1])
    # Validate colors early (also used for reporting).
    parse_hex_bgr(outer_color)
    parse_hex_bgr(inner_color)
    if not math.isfinite(contrast_threshold) or contrast_threshold <= 0.0:
        raise EvaluationError("contrast_threshold must be positive and finite")

    polygons = [instance.polygon for instance in predictions]
    clutter = clutter_fraction(polygons, height=height, width=width)
    ratios = local_stroke_contrast_samples(
        frame_bgr,
        polygons,
        outer_color=outer_color,
        sample_spacing_px=sample_spacing_px,
        outward_offset_px=outward_offset_px,
        background_radius_px=background_radius_px,
    )
    if ratios:
        passes = sum(1 for ratio in ratios if ratio >= contrast_threshold)
        ge3_fraction = float(passes) / float(len(ratios))
    else:
        ge3_fraction = 0.0

    return ComfortEvaluationResult(
        schema_version=SCHEMA_VERSION,
        frame_width=width,
        frame_height=height,
        prediction_count=len(predictions),
        clutter_fraction=clutter,
        local_stroke_contrast_ge_3_fraction=ge3_fraction,
        contrast_sample_count=len(ratios),
        outer_color=outer_color,
        inner_color=inner_color,
        contrast_threshold=float(contrast_threshold),
    )


def load_comfort_predictions_json(
    path: str | Path,
    *,
    width: int,
    height: int,
) -> tuple[PolygonInstance, ...]:
    """Load predictions for comfort eval.

    Accepts either::

        {"schema_version": 1, "predictions": [...]}

    or a bare list of prediction objects with absolute pixel polygons.
    """
    candidate = Path(path)
    if candidate.is_symlink() or not candidate.is_file():
        raise EvaluationError("predictions path must be a regular local file")
    if candidate.suffix.lower() != ".json":
        raise EvaluationError("predictions path must end with .json")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise EvaluationError(f"could not read predictions JSON: {error}") from error

    if isinstance(payload, list):
        raw_list = payload
    elif isinstance(payload, dict):
        version = payload.get("schema_version", SCHEMA_VERSION)
        if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
            raise EvaluationError(f"predictions schema_version must be {SCHEMA_VERSION}")
        if "predictions" in payload:
            raw_list = payload["predictions"]
        elif "images" in payload:
            images = payload["images"]
            if not isinstance(images, dict) or len(images) != 1:
                raise EvaluationError("comfort predictions.images must contain exactly one stem")
            only = next(iter(images.values()))
            if not isinstance(only, dict):
                raise EvaluationError("image entry must be an object")
            raw_list = only.get("predictions", [])
        else:
            raise EvaluationError("predictions object needs a predictions list or images map")
        if not isinstance(raw_list, list):
            raise EvaluationError("predictions must be a list")
    else:
        raise EvaluationError("predictions JSON root must be an object or list")

    if width <= 0 or height <= 0:
        raise EvaluationError("width and height must be positive")

    return tuple(
        _parse_prediction_instance(
            item,
            image_id="comfort",
            index=index,
            width=width,
            height=height,
        )
        for index, item in enumerate(raw_list)
    )


def evaluate_comfort_from_files(
    *,
    frame_path: str | Path,
    predictions_path: str | Path,
    outer_color: str = "#101010",
    inner_color: str = "#F6FF00",
    contrast_threshold: float = DEFAULT_STROKE_CONTRAST_THRESHOLD,
) -> ComfortEvaluationResult:
    """Load a PNG frame + predictions JSON and compute comfort proxies."""
    frame_bgr = _load_frame_bgr(frame_path)
    height, width = frame_bgr.shape[:2]
    predictions = load_comfort_predictions_json(
        predictions_path,
        width=width,
        height=height,
    )
    return evaluate_comfort(
        frame_bgr,
        predictions,
        outer_color=outer_color,
        inner_color=inner_color,
        contrast_threshold=contrast_threshold,
    )


def _edge_samples(
    polygon: Sequence[Point],
    *,
    spacing_px: float,
) -> list[tuple[float, float, float, float]]:
    """Sample points along polygon edges with outward unit normals."""
    points = list(polygon)
    if len(points) < 3:
        return []
    # Signed area to orient outward normal (positive CCW → outward is right of edge).
    area = 0.0
    for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1], strict=False):
        area += x1 * y2 - x2 * y1
    # Outward normal for CCW polygon: rotate edge tangent by +90° → (dy, -dx).
    # For CW polygon (area < 0), flip.
    sign = 1.0 if area >= 0.0 else -1.0

    samples: list[tuple[float, float, float, float]] = []
    for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1], strict=False):
        dx = float(x2) - float(x1)
        dy = float(y2) - float(y1)
        length = math.hypot(dx, dy)
        if length < 1e-9:
            continue
        tx = dx / length
        ty = dy / length
        # Perpendicular; sign chooses outward.
        nx = sign * ty
        ny = sign * (-tx)
        n_len = math.hypot(nx, ny)
        if n_len < 1e-9:
            continue
        nx /= n_len
        ny /= n_len
        count = max(1, int(math.floor(length / spacing_px)))
        for step in range(count):
            t = (step + 0.5) / count
            sx = float(x1) + t * dx
            sy = float(y1) + t * dy
            samples.append((sx, sy, nx, ny))
    return samples


def _sample_background_luminance(
    frame_bgr: np.ndarray,
    x: float,
    y: float,
    *,
    radius: int,
) -> float | None:
    height, width = frame_bgr.shape[:2]
    col = int(round(x))
    row = int(round(y))
    if col < 0 or row < 0 or col >= width or row >= height:
        return None
    r0 = max(0, row - radius)
    r1 = min(height, row + radius + 1)
    c0 = max(0, col - radius)
    c1 = min(width, col + radius + 1)
    patch = frame_bgr[r0:r1, c0:c1]
    if patch.size == 0:
        return None
    # Mean BGR then relative luminance.
    mean_bgr = patch.reshape(-1, 3).mean(axis=0)
    return _bgr_relative_luminance(
        float(mean_bgr[0]),
        float(mean_bgr[1]),
        float(mean_bgr[2]),
    )
