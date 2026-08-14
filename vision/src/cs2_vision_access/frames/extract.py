"""Frame extraction logic — OpenCV-driven deterministic frame saving.

Separated from ``models.py`` so that model consumers do not need OpenCV.
"""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

from cs2_vision_access.frames.models import (
    SCHEMA_VERSION,
    SESSION_FILENAME,
    FrameExtractionError,
    FrameExtractionSummary,
    FramePolicy,
    RightsPlaceholder,
    SessionProvenance,
)
from cs2_vision_access.safety import (
    InputSafetyError,
    validate_decoded_frame,
    validate_video_input,
)


def extract_frames(
    input_path: str | Path,
    output_directory: str | Path,
    *,
    every_n_frames: int,
    max_saved_frames: int,
    session_id: str | None = None,
    capture_notes: str = "",
    rights: RightsPlaceholder | None = None,
) -> FrameExtractionSummary:
    """Extract evenly-spaced frames from a video, writing PNGs and provenance metadata.

    Args:
        input_path: Path to the source video file.
        output_directory: Empty directory to receive frames and ``session.json``.
        every_n_frames: Extract one frame every N decoded frames.
        max_saved_frames: Maximum number of frames to save.
        session_id: Identifier for this extraction session. Defaults to directory name.
        capture_notes: Free-text provenance note.

    Returns:
        Summary of what was extracted.
    """
    if every_n_frames <= 0:
        raise ValueError("every_n_frames must be positive")
    if max_saved_frames <= 0:
        raise ValueError("max_saved_frames must be positive")
    if not isinstance(capture_notes, str):
        raise ValueError("capture_notes must be a string")

    source = validate_video_input(input_path)
    destination = Path(output_directory)
    if destination.is_symlink():
        raise FrameExtractionError("output directory must not be a symlink")
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise FrameExtractionError("output directory must be empty to prevent accidental overwrite")
    destination = destination.resolve()

    resolved_session_id = (
        session_id.strip()
        if isinstance(session_id, str) and session_id.strip()
        else destination.name
    )
    if not resolved_session_id:
        raise FrameExtractionError("session_id must be a non-empty string")

    try:
        import cv2
    except ImportError as error:
        raise FrameExtractionError(
            "OpenCV is required for frame extraction; install project dependencies"
        ) from error

    capture = cv2.VideoCapture(str(source))
    decoded = saved = 0
    source_dimensions: tuple[int, int] | None = None
    try:
        if not capture.isOpened():
            raise FrameExtractionError(f"OpenCV could not open video: {source}")
        while saved < max_saved_frames:
            ok, frame = capture.read()
            if not ok:
                break
            try:
                source_dimensions = validate_decoded_frame(
                    frame,
                    expected_dimensions=source_dimensions,
                )
            except InputSafetyError as error:
                raise FrameExtractionError(str(error)) from error
            if decoded % every_n_frames == 0:
                filename = f"{source.stem}__f{decoded:09d}.png"
                if not cv2.imwrite(str(destination / filename), frame):
                    raise FrameExtractionError(f"could not write frame {filename}")
                saved += 1
            decoded += 1
    finally:
        capture.release()
    if decoded == 0:
        raise FrameExtractionError("video contained no decodable frames")

    resolved_rights = rights if rights is not None else RightsPlaceholder.placeholder()
    provenance = SessionProvenance(
        schema_version=SCHEMA_VERSION,
        session_id=resolved_session_id,
        source_stem=source.stem,
        rights=resolved_rights,
        capture_notes=capture_notes,
        frame_policy=FramePolicy(
            every_n_frames=every_n_frames,
            max_saved_frames=max_saved_frames,
            decoded_frames=decoded,
            saved_frames=saved,
        ),
    )
    SessionProvenance.from_mapping(provenance.as_json())
    session_path = _write_session_json(destination / SESSION_FILENAME, provenance)

    return FrameExtractionSummary(
        decoded_frames=decoded,
        saved_frames=saved,
        output_directory=str(destination),
        session_id=resolved_session_id,
        source_stem=source.stem,
        session_path=str(session_path),
    )


def _write_session_json(destination: Path, provenance: SessionProvenance) -> Path:
    """Atomically write a session.json provenance sidecar."""
    if destination.is_symlink():
        raise FrameExtractionError("session.json destination must not be a symlink")
    if destination.exists():
        raise FrameExtractionError(f"session.json already exists: {destination}")
    payload = json.dumps(provenance.as_json(), indent=2, sort_keys=True) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(payload)
            handle.flush()
        Path(temporary_name).replace(destination)
    except OSError as error:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
        raise FrameExtractionError(f"could not write session.json: {error}") from error
    return destination.resolve()
