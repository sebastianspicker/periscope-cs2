"""Ledger L2 tests: phone-camera capture source (no phone, no hardware)."""

from __future__ import annotations

import numpy as np
import pytest

from periscope_lens.phone import (
    SUPPORTED_PHONE_BACKENDS,
    PhoneCameraSource,
    PhoneSourceError,
    build_scrcpy_command,
    describe_source,
    validate_backend,
)


def test_scrcpy_command_composition() -> None:
    command = build_scrcpy_command(device_serial="ABCD123", camera_id="2")
    assert command[0] == "scrcpy"
    assert "--video-source=camera" in command, "the camera, not the screen"
    assert "--no-window" in command and "--no-audio" in command
    assert ["--serial", "ABCD123"] in [command[i : i + 2] for i in range(len(command) - 1)]
    assert "--camera-id=2" in command
    assert "--max-size=1280" in command and "--max-fps=60" in command


def test_scrcpy_command_validation() -> None:
    with pytest.raises(PhoneSourceError):
        build_scrcpy_command(max_size=10)
    with pytest.raises(PhoneSourceError):
        build_scrcpy_command(max_fps=0)
    with pytest.raises(PhoneSourceError):
        build_scrcpy_command(max_fps=1000)


def test_backend_validation() -> None:
    assert validate_backend("scrcpy") == "scrcpy"
    assert validate_backend("uvc") == "uvc"
    assert validate_backend("droidcam") == "droidcam"
    with pytest.raises(PhoneSourceError, match="unsupported phone backend"):
        validate_backend("airdrop")
    assert frozenset({"scrcpy", "uvc", "droidcam"}) == SUPPORTED_PHONE_BACKENDS


def _source(
    reads: list[tuple[bool, np.ndarray | None]], failures_allowed: int = 3
) -> tuple[PhoneCameraSource, list[int]]:
    requested: list[int] = []

    def reader(frame_index: int) -> tuple[bool, np.ndarray | None, str]:
        requested.append(frame_index)
        index = len(requested) - 1
        if index < len(reads):
            ok, frame = reads[index]
            return (ok, frame, "")
        return (False, None, "")  # scripted reads exhausted: dropped read

    return PhoneCameraSource(
        backend="uvc",
        frame_reader=reader,
        max_consecutive_failures=failures_allowed,
    ), requested


def test_read_loop_contract() -> None:
    frame = np.zeros((4, 8, 3), dtype=np.uint8)
    source, requested = _source(
        [
            (False, None),  # dropped read (transient)
            (True, frame),  # good read resets the counter
            (False, None),  # dropped again
        ]
    )
    assert source.read(0) == (False, None, ""), "A4: a live camera never EOFs"
    assert source.consecutive_failures == 1
    assert source.read(1) == (True, frame, "")
    assert source.consecutive_failures == 0, "B8: counter resets after success"
    assert source.read(2) == (False, None, "")
    assert source.consecutive_failures == 1
    assert requested == [0, 1, 2], "frame indices forwarded for diagnostics"
    assert not source.should_reconnect()


def test_reconnect_threshold_and_reset() -> None:
    source, _ = _source([(False, None)] * 10, failures_allowed=3)
    for index in range(2):
        source.read(index)
    assert not source.should_reconnect()
    source.read(3)
    assert source.should_reconnect()
    source.reset_after_reconnect()
    assert not source.should_reconnect()
    assert source.consecutive_failures == 0


def test_closed_source_stops_cleanly() -> None:
    source, requested = _source([])
    source.close()
    assert source.read(0) == (False, None, "phone_source_closed")
    # A clean close is terminal and is not counted as a failure, so the
    # caller is not pushed into a reconnect loop by the close itself.
    assert source.consecutive_failures == 0
    assert not source.should_reconnect()
    assert requested == [], "closed source never reaches the reader"


def test_close_releases_attached_reader_handle() -> None:
    released: list[bool] = []

    def reader(frame_index: int) -> tuple[bool, np.ndarray | None, str]:
        del frame_index
        return (False, None, "")

    reader.close = lambda: released.append(True)  # type: ignore[attr-defined]
    source = PhoneCameraSource(backend="scrcpy", frame_reader=reader)
    source.close()
    assert released == [True], "closer invoked on close"
    source.close()
    assert released == [True, True], "close stays callable"


def test_source_validation_and_summary() -> None:
    with pytest.raises(PhoneSourceError):
        PhoneCameraSource(backend="carrier-pigeon", frame_reader=lambda i: (True, None, ""))
    with pytest.raises(PhoneSourceError):
        PhoneCameraSource(
            backend="uvc", frame_reader=lambda i: (True, None, ""), max_consecutive_failures=0
        )
    frame = np.zeros((2, 2, 3), dtype=np.uint8)
    source = PhoneCameraSource(backend="droidcam", frame_reader=lambda i: (True, frame, ""))
    summary = describe_source(source)
    assert summary["backend"] == "droidcam"
    assert summary["consecutive_failures"] == 0
    assert summary["closed"] is False
