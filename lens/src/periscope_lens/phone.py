"""Phone-camera capture source (ledger L2, topology A).

The smartphone is pointed at the game monitor and acts as the capture
device. Three transports, in preference order (see `lens/docs/TOPOLOGY.md`):

- ``scrcpy``, the PC pulls the Android device's camera over ADB/USB
  (scrcpy ``--video-source=camera``); no extra phone app, no UVC driver.
  scrcpy is Apache-2.0 and is *invoked*, never linked or bundled.
- ``uvc``, the phone runs in its vendor webcam mode; the PC opens it as a
  standard camera device (OpenCV capture, same as any webcam).
- ``droidcam``, user-installed proprietary client; supported as an option,
  never bundled.

The portable half of this module is everything testable without a phone:

- backend validation and the scrcpy command line (pure, unit-tested),
- the read-loop contract (``(ok, frame, reason)`` with explicit
  disconnect/reconnect semantics, a live camera never "ends", the A4
  lesson, and the failure counter resets after a good read, the B8 lesson),
- latency-probe hooks for L7.

The real cv2/scrcpy bindings are thin factories that follow the same
runtime-unverified pattern as the other real backends in this package.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

import numpy as np

SUPPORTED_PHONE_BACKENDS = frozenset({"scrcpy", "uvc", "droidcam"})
_DEFAULT_SCRCPY_MAX_SIZE = 1280
_DEFAULT_SCRCPY_MAX_FPS = 60


class PhoneSourceError(ValueError):
    """Phone-camera source configuration or runtime failure."""


def build_scrcpy_command(
    *,
    device_serial: str | None = None,
    max_size: int = _DEFAULT_SCRCPY_MAX_SIZE,
    max_fps: int = _DEFAULT_SCRCPY_MAX_FPS,
    camera_id: str | None = None,
    v4l2_sink: str | None = None,
    record_path: str = "raw_video.h264",
) -> list[str]:
    """The scrcpy invocation that streams the phone's *camera* (not screen).

    Pure: composition only, no subprocess. ``--no-window`` keeps the feed
    headless; ``--no-audio`` because the pipeline consumes frames only.
    """
    if max_size < 64:
        raise PhoneSourceError("max_size must be at least 64")
    if not 1 <= max_fps <= 240:
        raise PhoneSourceError("max_fps must be in [1, 240]")
    command = ["scrcpy", "--video-source=camera", "--no-window", "--no-audio"]
    if camera_id is not None:
        command.append(f"--camera-id={camera_id}")
    if device_serial is not None:
        command.extend(["--serial", device_serial])
    if v4l2_sink is not None:
        command.append(f"--v4l2-sink={v4l2_sink}")
    command.extend(
        [
            f"--max-size={max_size}",
            f"--max-fps={max_fps}",
            f"--record={record_path}",
            "--no-playback",
        ]
    )
    return command


def validate_backend(backend: str) -> str:
    if backend not in SUPPORTED_PHONE_BACKENDS:
        supported = ", ".join(sorted(SUPPORTED_PHONE_BACKENDS))
        raise PhoneSourceError(f"unsupported phone backend {backend!r}; supported: {supported}")
    return backend


FrameReader = Callable[[int], tuple[bool, np.ndarray | None, str]]


@dataclass
class PhoneCameraSource:
    """Protocol-compatible capture source (``read(index)`` triple contract).

    The injected ``frame_reader`` is the transport's per-call read (UVC
    ``cap.read()`` wrapper, scrcpy pipe demux, …). This class owns the
    loop semantics every transport shares:

    - a live camera never ends: a failed read returns ``(False, None, "")``
      for the caller to count, not an EOF (A4);
    - the failure counter resets after a good read (B8): a few dropped
      reads spread over an hour must not trigger a reconnect storm;
    - the caller decides on reconnects via ``should_reconnect`` after
      ``max_consecutive_failures`` *consecutive* failures.
    """

    backend: str
    frame_reader: FrameReader
    reopen: Callable[[], FrameReader] | None = None
    max_consecutive_failures: int = 3
    _failures: int = 0
    _closed: bool = field(default=False)

    def __post_init__(self) -> None:
        validate_backend(self.backend)
        if self.max_consecutive_failures < 1:
            raise PhoneSourceError("max_consecutive_failures must be >= 1")

    def read(self, frame_index: int) -> tuple[bool, np.ndarray | None, str]:
        """One read with the shared capture contract.

                Returns ``(True, frame, "")`` on success, ``(False, None, "")`` on a
                dropped read (failure counter incremented), and
                ``(False, None, reason)`` only when this source decided to stop
                (explicit close); reconnect decisions belong to the caller via
        :meth:`should_reconnect`. Reasons *returned by the reader* are
                advisory and dropped here: a live camera has no end-of-stream, so a
                per-read reason would otherwise terminate the pipeline (A4).
        """
        if self._closed:
            return (False, None, "phone_source_closed")
        ok, frame, reason = self.frame_reader(frame_index)
        if ok and frame is not None:
            self._failures = 0  # B8: reset after success, not on reconnect
            return (True, frame, "")
        self._failures += 1
        return (False, None, "")

    @property
    def consecutive_failures(self) -> int:
        return self._failures

    @property
    def closed(self) -> bool:
        return self._closed

    def should_reconnect(self) -> bool:
        return self._failures >= self.max_consecutive_failures

    def reconnect(self) -> bool:
        """Release the current handle and rebuild it through ``reopen``.

        Returns false when no factory is configured or reopen fails. The
        failure counter is only cleared after a successful rebuild.
        """
        closer = getattr(self.frame_reader, "close", None)
        if callable(closer):
            closer()
        if self.reopen is None:
            self._failures = 0
            return False
        try:
            self.frame_reader = self.reopen()
        except PhoneSourceError:
            return False
        self._failures = 0
        self._closed = False
        return True

    def reset_after_reconnect(self) -> None:
        self._failures = 0

    def close(self) -> None:
        self._closed = True
        # The transport factories attach the capture handle's release as a
        # ``close`` attribute on the reader function; release it so a
        # reconnect does not leak device handles.
        closer = getattr(self.frame_reader, "close", None)
        if callable(closer):
            closer()


def _import_cv2() -> Any:
    """OpenCV is an optional `capture` extra, not a default dependency."""
    try:
        import cv2
    except ImportError as error:  # pragma: no cover - exercised without the extra
        raise PhoneSourceError(
            "OpenCV is required for the real phone bindings; install it with "
            "`uv sync --extra capture`"
        ) from error
    return cv2


def make_uvc_reader(device_index: int) -> FrameReader:
    """Binds a UVC/webcam device through OpenCV (runtime-unverified real
    binding, same posture as the other real capture backends)."""
    cv2 = _import_cv2()

    capture = cv2.VideoCapture(device_index)
    if not capture.isOpened():
        raise PhoneSourceError(f"cannot open UVC device {device_index}")

    def reader(frame_index: int) -> tuple[bool, np.ndarray | None, str]:
        del frame_index  # UVC reads are sequential; the index is diagnostic
        ok, frame = capture.read()
        return (bool(ok), frame if ok else None, "")

    reader.close = capture.release  # type: ignore[attr-defined]
    return reader


def open_stream_reader(stream_url: str) -> FrameReader:
    """Binds whatever URL the transport produces through OpenCV.

    Examples: a UVC device path, a scrcpy v4l2 sink device, or a DroidCam
    HTTP/MJPEG URL. One honest binding instead of per-backend fakery; the
    scrcpy process itself (started with :func:`build_scrcpy_command` and
    pointed at its sink) is launched by the lab runner, not here.
    """
    cv2 = _import_cv2()

    capture = cv2.VideoCapture(stream_url)
    if not capture.isOpened():
        raise PhoneSourceError(f"cannot open phone stream {stream_url!r}")

    def reader(frame_index: int) -> tuple[bool, np.ndarray | None, str]:
        del frame_index
        ok, frame = capture.read()
        return (bool(ok), frame if ok else None, "")

    reader.close = capture.release  # type: ignore[attr-defined]
    return reader


def describe_source(source: PhoneCameraSource) -> dict[str, Any]:
    """Telemetry-friendly summary; the actual backend, never the requested
    one (the honesty rule carried over from V3)."""
    return {
        "backend": source.backend,
        "consecutive_failures": source.consecutive_failures,
        "closed": source.closed,
    }
