"""Frame image loading for evaluation (OpenCV + stdlib PNG fallback)."""

from __future__ import annotations

from pathlib import Path
from typing import Any, cast

import numpy as np

from cs2_vision_access.workflows.evaluation.errors import EvaluationError


def _load_frame_bgr(path: str | Path) -> np.ndarray:
    """Load a frame as HxWx3 uint8 BGR.

    Prefers OpenCV when installed. Falls back to a stdlib PNG decoder for
    8-bit RGB/RGBA/grayscale PNGs so comfort metrics stay dependency-light in
    unit tests.
    """
    candidate = Path(path)
    if candidate.is_symlink() or not candidate.is_file():
        raise EvaluationError("frame path must be a regular local file")
    suffix = candidate.suffix.lower()
    if suffix not in {".png", ".jpg", ".jpeg", ".bmp", ".webp"}:
        raise EvaluationError("frame path must be an image file (png/jpg/bmp/webp)")

    cv2: Any | None
    try:
        import cv2 as cv2_module
    except ImportError:
        cv2 = None
    else:
        cv2 = cv2_module
    if cv2 is not None:
        image = cv2.imread(str(candidate), cv2.IMREAD_COLOR)
        if image is not None and image.ndim == 3 and image.shape[2] == 3:
            return cast(np.ndarray, image)

    if suffix == ".png":
        return _load_png_bgr(candidate)

    if cv2 is None:
        raise EvaluationError(
            "OpenCV is required to load non-PNG frames; install opencv-python or provide a PNG"
        )
    raise EvaluationError(f"could not decode frame image: {candidate}")


def _load_png_bgr(path: Path) -> np.ndarray:
    """Decode an 8-bit PNG to BGR using only the standard library + numpy."""
    import struct
    import zlib

    try:
        data = path.read_bytes()
    except OSError as error:
        raise EvaluationError(f"could not read frame image: {error}") from error
    if len(data) < 8 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise EvaluationError(f"not a PNG file: {path}")

    offset = 8
    width = height = None
    bit_depth = color_type = None
    idat = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        start = offset + 8
        end = start + length
        if end + 4 > len(data):
            raise EvaluationError(f"truncated PNG chunk in {path}")
        chunk = data[start:end]
        offset = end + 4  # skip CRC
        if chunk_type == b"IHDR":
            if length < 13:
                raise EvaluationError(f"invalid PNG IHDR in {path}")
            width, height, bit_depth, color_type, *_rest = struct.unpack(">IIBBBBB", chunk[:13])
        elif chunk_type == b"IDAT":
            idat.extend(chunk)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None or bit_depth is None or color_type is None:
        raise EvaluationError(f"PNG missing IHDR: {path}")
    if bit_depth != 8:
        raise EvaluationError("stdlib PNG fallback supports only 8-bit images")
    if color_type not in {0, 2, 4, 6}:
        raise EvaluationError("stdlib PNG fallback supports grayscale, RGB, or RGBA only")
    try:
        raw = zlib.decompress(bytes(idat))
    except zlib.error as error:
        raise EvaluationError(f"could not decompress PNG: {error}") from error

    if color_type == 0:
        channels = 1
    elif color_type == 2:
        channels = 3
    elif color_type == 4:
        channels = 2
    else:
        channels = 4
    row_bytes = 1 + width * channels
    expected = row_bytes * height
    if len(raw) < expected:
        raise EvaluationError(f"PNG image data too short: {path}")

    rows: list[bytearray] = []
    for row_index in range(height):
        row_start = row_index * row_bytes
        filter_type = raw[row_start]
        row = bytearray(raw[row_start + 1 : row_start + row_bytes])
        if filter_type == 0:
            pass
        elif filter_type == 1:  # Sub
            for i in range(channels, len(row)):
                row[i] = (row[i] + row[i - channels]) & 0xFF
        elif filter_type == 2:  # Up
            if row_index == 0:
                raise EvaluationError("PNG Up filter on first row")
            previous_row = rows[row_index - 1]
            for i in range(len(row)):
                row[i] = (row[i] + previous_row[i]) & 0xFF
        elif filter_type == 3:  # Average
            average_previous: bytearray | None = rows[row_index - 1] if row_index else None
            for i in range(len(row)):
                left = row[i - channels] if i >= channels else 0
                up = average_previous[i] if average_previous is not None else 0
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
        elif filter_type == 4:  # Paeth
            paeth_previous: bytearray | None = rows[row_index - 1] if row_index else None
            for i in range(len(row)):
                left = row[i - channels] if i >= channels else 0
                up = paeth_previous[i] if paeth_previous is not None else 0
                up_left = (
                    paeth_previous[i - channels]
                    if paeth_previous is not None and i >= channels
                    else 0
                )
                row[i] = (row[i] + _paeth_predictor(left, up, up_left)) & 0xFF
        else:
            raise EvaluationError(f"unsupported PNG filter type {filter_type}")
        rows.append(row)

    flat = np.frombuffer(b"".join(rows), dtype=np.uint8)
    if channels == 1:
        gray = flat.reshape((height, width))
        rgb = np.stack([gray, gray, gray], axis=-1)
    elif channels == 2:
        gray = flat.reshape((height, width, 2))[:, :, 0]
        rgb = np.stack([gray, gray, gray], axis=-1)
    elif channels == 3:
        rgb = flat.reshape((height, width, 3))
    else:
        rgb = flat.reshape((height, width, 4))[:, :, :3]
    # RGB → BGR
    return rgb[:, :, ::-1].copy()


def _paeth_predictor(left: int, up: int, up_left: int) -> int:
    estimate = left + up - up_left
    distance_left = abs(estimate - left)
    distance_up = abs(estimate - up)
    distance_up_left = abs(estimate - up_left)
    if distance_left <= distance_up and distance_left <= distance_up_left:
        return left
    if distance_up <= distance_up_left:
        return up
    return up_left
