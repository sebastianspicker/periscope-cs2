"""File-only runtime boundary and media path validation."""

from __future__ import annotations

from pathlib import Path

VIDEO_EXTENSIONS = frozenset({".avi", ".mkv", ".mp4", ".mov", ".webm"})
MAX_VIDEO_BYTES = 100_000_000_000
MAX_FRAME_PIXELS = 3840 * 2160


class InputSafetyError(ValueError):
    """Input violates the repository's file-only safety contract."""


def validate_video_input(path: str | Path, *, max_video_bytes: int = MAX_VIDEO_BYTES) -> Path:
    if max_video_bytes <= 0:
        raise ValueError("max_video_bytes must be positive")
    candidate = Path(path)
    if candidate.is_symlink():
        raise InputSafetyError("video input must not be a symlink")
    if not candidate.is_file():
        raise InputSafetyError(f"video input is not a local file: {candidate}")
    if candidate.suffix.lower() not in VIDEO_EXTENSIONS:
        supported = ", ".join(sorted(VIDEO_EXTENSIONS))
        raise InputSafetyError(f"unsupported video extension; expected one of: {supported}")
    if candidate.stat().st_size > max_video_bytes:
        raise InputSafetyError(f"video input exceeds the {max_video_bytes}-byte safety limit")
    return candidate.resolve()


def validate_video_output(path: str | Path, *, overwrite: bool) -> Path:
    candidate = Path(path)
    if candidate.suffix.lower() != ".mp4":
        raise InputSafetyError("rendered output must use the .mp4 extension")
    if candidate.is_symlink():
        raise InputSafetyError("video output must not be a symlink")
    if candidate.exists() and not candidate.is_file():
        raise InputSafetyError("video output exists and is not a regular file")
    if candidate.exists() and not overwrite:
        raise InputSafetyError(
            f"video output already exists: {candidate}; pass --overwrite to replace it"
        )
    candidate.parent.mkdir(parents=True, exist_ok=True)
    if candidate.parent.is_symlink():
        raise InputSafetyError("video output parent must not be a symlink")
    return candidate.resolve()


def validate_image_output(path: str | Path, *, overwrite: bool) -> Path:
    """Validate a local single-frame PNG destination (preview / A/B path)."""
    candidate = Path(path)
    if candidate.suffix.lower() != ".png":
        raise InputSafetyError("image output must use the .png extension")
    if candidate.is_symlink():
        raise InputSafetyError("image output must not be a symlink")
    if candidate.exists() and not candidate.is_file():
        raise InputSafetyError("image output exists and is not a regular file")
    if candidate.exists() and not overwrite:
        raise InputSafetyError(
            f"image output already exists: {candidate}; pass --overwrite to replace it"
        )
    candidate.parent.mkdir(parents=True, exist_ok=True)
    if candidate.parent.is_symlink():
        raise InputSafetyError("image output parent must not be a symlink")
    return candidate.resolve()


def validate_decoded_frame(
    frame: object,
    *,
    expected_dimensions: tuple[int, int] | None = None,
    max_frame_pixels: int = MAX_FRAME_PIXELS,
) -> tuple[int, int]:
    """Validate actual decoded pixels rather than fallible container metadata."""
    if max_frame_pixels <= 0:
        raise ValueError("max_frame_pixels must be positive")
    shape = getattr(frame, "shape", None)
    if (
        not isinstance(shape, tuple)
        or len(shape) != 3
        or not all(isinstance(value, int) for value in shape)
    ):
        raise InputSafetyError("decoded frame must have an integer (height, width, channels) shape")
    height, width, channels = shape
    if height <= 0 or width <= 0 or channels != 3:
        raise InputSafetyError(
            f"unsupported decoded frame shape: {shape}; expected three BGR channels"
        )
    if width * height > max_frame_pixels:
        raise InputSafetyError(
            f"unsupported decoded dimensions: {width}x{height}; "
            f"maximum pixels are {max_frame_pixels}"
        )
    dimensions = (width, height)
    if expected_dimensions is not None and dimensions != expected_dimensions:
        expected_width, expected_height = expected_dimensions
        raise InputSafetyError(
            "decoded frame dimensions changed from "
            f"{expected_width}x{expected_height} to {width}x{height}"
        )
    return dimensions
