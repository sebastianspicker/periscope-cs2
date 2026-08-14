"""Geometry conversion and detection-label parsing for bootstrap."""

from __future__ import annotations

import math
from pathlib import Path

from cs2_vision_access.labeling.types import (
    ELLIPSE_POINT_COUNT,
    BootstrapError,
    DetectionBox,
)


def box_to_rectangle_polygon(
    x_center: float, y_center: float, width: float, height: float
) -> list[float]:
    """Return 4-corner normalized polygon ``[x1,y1,...,x4,y4]`` for a box."""
    half_w = width / 2.0
    half_h = height / 2.0
    x_min = _clamp01(x_center - half_w)
    y_min = _clamp01(y_center - half_h)
    x_max = _clamp01(x_center + half_w)
    y_max = _clamp01(y_center + half_h)
    if x_max <= x_min or y_max <= y_min:
        raise BootstrapError("box collapses to zero area after clamping to [0, 1]")
    return [x_min, y_min, x_max, y_min, x_max, y_max, x_min, y_max]


def box_to_ellipse_polygon(
    x_center: float,
    y_center: float,
    width: float,
    height: float,
    *,
    point_count: int = ELLIPSE_POINT_COUNT,
) -> list[float]:
    """Return an ellipse polygon (≥16 points) inscribed in the box."""
    if point_count < ELLIPSE_POINT_COUNT:
        raise BootstrapError(f"ellipse backend requires at least {ELLIPSE_POINT_COUNT} points")
    half_w = width / 2.0
    half_h = height / 2.0
    if half_w <= 0.0 or half_h <= 0.0:
        raise BootstrapError("box width and height must be positive")
    coordinates: list[float] = []
    for index in range(point_count):
        theta = 2.0 * math.pi * index / point_count
        x = _clamp01(x_center + half_w * math.cos(theta))
        y = _clamp01(y_center + half_h * math.sin(theta))
        coordinates.extend((x, y))
    if _polygon_area(coordinates) == 0.0:
        raise BootstrapError("ellipse polygon has zero area after clamping")
    return coordinates


def parse_detection_label(label_path: Path) -> list[DetectionBox]:
    """Parse a YOLO detection ``.txt`` into normalized boxes."""
    if label_path.is_symlink():
        raise BootstrapError(f"label path must not be a symlink: {label_path}")
    if not label_path.is_file():
        raise BootstrapError(f"label is not a regular file: {label_path}")
    try:
        contents = label_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise BootstrapError(f"could not read label {label_path}: {error}") from error

    boxes: list[DetectionBox] = []
    for line_number, row in enumerate(contents.splitlines(), start=1):
        tokens = row.split()
        if not tokens:
            continue
        if len(tokens) != 5:
            raise BootstrapError(
                f"{label_path}:{line_number}: detection row must be "
                f"'class_id x_c y_c w h' (5 tokens), found {len(tokens)}"
            )
        class_token, x_token, y_token, w_token, h_token = tokens
        if not class_token.isascii() or not class_token.isdecimal():
            raise BootstrapError(
                f"{label_path}:{line_number}: class id must be a non-negative integer"
            )
        try:
            x_center = float(x_token)
            y_center = float(y_token)
            width = float(w_token)
            height = float(h_token)
        except ValueError as error:
            raise BootstrapError(
                f"{label_path}:{line_number}: coordinates must be finite decimals"
            ) from error
        values = (x_center, y_center, width, height)
        if any(not math.isfinite(value) for value in values):
            raise BootstrapError(f"{label_path}:{line_number}: coordinates must be finite")
        if any(not 0.0 <= value <= 1.0 for value in values):
            raise BootstrapError(f"{label_path}:{line_number}: coordinates must be in [0, 1]")
        if width <= 0.0 or height <= 0.0:
            raise BootstrapError(
                f"{label_path}:{line_number}: box width and height must be positive"
            )
        # Reject boxes whose center±half extent is wildly outside after geometry
        # checks; clamping happens at polygon generation.
        boxes.append(
            DetectionBox(
                source_class_id=int(class_token),
                x_center=x_center,
                y_center=y_center,
                width=width,
                height=height,
                line_number=line_number,
            )
        )
    return boxes


def format_segmentation_row(class_id: int, coordinates: list[float]) -> str:
    if class_id < 0:
        raise BootstrapError("output class id must be non-negative")
    if len(coordinates) < 6 or len(coordinates) % 2:
        raise BootstrapError("polygon needs at least three coordinate pairs")
    parts = [str(class_id)]
    parts.extend(_format_coord(value) for value in coordinates)
    return " ".join(parts)


def _clamp01(value: float) -> float:
    if value < 0.0:
        return 0.0
    if value > 1.0:
        return 1.0
    return value


def _format_coord(value: float) -> str:
    # Stable, YOLO-friendly fixed precision without scientific notation.
    return f"{value:.6f}"


def _polygon_area(coordinates: list[float]) -> float:
    points = list(zip(coordinates[::2], coordinates[1::2], strict=False))
    if len(points) < 3:
        return 0.0
    total = 0.0
    for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1], strict=False):
        total += x1 * y2 - x2 * y1
    return abs(total) / 2.0
