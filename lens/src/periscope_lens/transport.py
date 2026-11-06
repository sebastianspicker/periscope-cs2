"""USB transport protocol between phone and PC (ledger L6).

Binary, little-endian, versioned. Frame layout:

    u8 protocol_version, u8 message_type, u16 payload_length, payload

Carried over from the research ledger (R5/R6 doctrine, applied to the cable):

- **Versioned and self-describing**; unknown versions fail loudly (the V1
  clean-rejection rule), unknown *message types* are skipped (forward compat
  within a known version).
- **Absolute-state payloads**: each ``Targets`` frame is complete for its
  frame id — no deltas (decision 2 of the ledger).
- **Bounded**: a frame larger than 512 KiB is rejected.
- The capture timestamp in ``Targets`` is the *camera* timestamp
  (Camera2 ``sensorTimestamp``), not the send time — L7 compensation depends
  on it.
- u64 ids never cross the wire as floats; they stay integers end to end.

Pure encode/decode over ``bytes``; the socket (adb forward / RNDIS) is the
adapter's concern.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Any

PROTOCOL_VERSION = 1
MAX_FRAME_BYTES = 512 * 1024
HEADER = struct.Struct("<BBI")  # version, type, payload length (u32)

TYPE_HELLO = 1
TYPE_TARGETS = 2
TYPE_PING = 3
TYPE_PONG = 4
TYPE_CALIB = 5
TYPE_STATE = 6

_KNOWN_TYPES = frozenset({TYPE_HELLO, TYPE_TARGETS, TYPE_PING, TYPE_PONG, TYPE_CALIB, TYPE_STATE})
_KNOWN_VERSIONS = frozenset({PROTOCOL_VERSION})

_HELLO = struct.Struct("<HI")  # app_build, flags
_PING = struct.Struct("<Q")  # t0_ns
_TARGETS_HEAD = struct.Struct("<QQB")  # frame_id, t_capture_ns, count
_TARGET_BOX = struct.Struct("<Bfffff")  # class_id, conf, cx, cy, w, h
_CALIB = struct.Struct("<9fQf")  # h[9], t_ns, rms
_STATE = struct.Struct("<HHHHHHB")  # fps_meas, fps_tgt, roi, flags


class ProtocolError(ValueError):
    """The wire contract was violated; the peer must not be trusted further."""


@dataclass(frozen=True)
class Hello:
    app_build: int
    flags: int


@dataclass(frozen=True)
class TargetBox:
    class_id: int
    confidence: float
    cx: float
    cy: float
    width: float
    height: float


@dataclass(frozen=True)
class Targets:
    frame_id: int
    t_capture_ns: int
    boxes: tuple[TargetBox, ...]


@dataclass(frozen=True)
class Ping:
    t0_ns: int
    is_reply: bool


@dataclass(frozen=True)
class Calib:
    matrix: tuple[tuple[float, float, float], ...]
    t_ns: int
    rms: float


@dataclass(frozen=True)
class State:
    fps_measured: int
    fps_target: int
    roi_x: int
    roi_y: int
    roi_w: int
    roi_h: int
    flags: int


def encode(message: Hello | Targets | Ping | Calib | State) -> bytes:
    """Encodes one frame including the header."""
    if isinstance(message, Hello):
        payload = _HELLO.pack(message.app_build, message.flags)
        return _frame(TYPE_HELLO, payload)
    if isinstance(message, Targets):
        if len(message.boxes) > 255:
            raise ProtocolError(f"{len(message.boxes)} boxes exceed the u8 count field")
        head = _TARGETS_HEAD.pack(message.frame_id, message.t_capture_ns, len(message.boxes))
        body = b"".join(
            _TARGET_BOX.pack(box.class_id, box.confidence, box.cx, box.cy, box.width, box.height)
            for box in message.boxes
        )
        return _frame(TYPE_TARGETS, head + body)
    if isinstance(message, Ping):
        return _frame(TYPE_PONG if message.is_reply else TYPE_PING, _PING.pack(message.t0_ns))
    if isinstance(message, Calib):
        flat = tuple(value for row in message.matrix for value in row)
        payload = _CALIB.pack(*flat, message.t_ns, message.rms)
        return _frame(TYPE_CALIB, payload)
    if isinstance(message, State):
        payload = _STATE.pack(
            message.fps_measured,
            message.fps_target,
            message.roi_x,
            message.roi_y,
            message.roi_w,
            message.roi_h,
            message.flags,
        )
        return _frame(TYPE_STATE, payload)
    raise ProtocolError(f"unsupported message: {message!r}")


def _frame(message_type: int, payload: bytes) -> bytes:
    return HEADER.pack(PROTOCOL_VERSION, message_type, len(payload)) + payload


class Decoder:
    """Streaming decoder over concatenated frames.

    ``feed(chunk)`` appends bytes; ``messages()`` decodes everything
    buffered. Unknown message types are skipped with their payload (forward
    compat); a wrong version, a truncated frame, or an oversized frame raise
    :class:`ProtocolError`.
    """

    def __init__(self) -> None:
        self._buffer = b""

    def feed(self, chunk: bytes) -> None:
        self._buffer += chunk
        if len(self._buffer) > MAX_FRAME_BYTES * 2:
            raise ProtocolError("decoder buffer overrun")

    def messages(self) -> list[Hello | Targets | Ping | Calib | State | None]:
        decoded: list[Hello | Targets | Ping | Calib | State | None] = []
        while True:
            if len(self._buffer) < HEADER.size:
                break
            version, message_type, length = HEADER.unpack_from(self._buffer, 0)
            if version not in _KNOWN_VERSIONS:
                raise ProtocolError(f"unsupported protocol version {version}")
            if length > MAX_FRAME_BYTES:
                raise ProtocolError(f"frame of {length} bytes exceeds cap")
            total = HEADER.size + length
            if len(self._buffer) < total:
                break  # wait for the rest of the frame
            payload = self._buffer[HEADER.size : total]
            self._buffer = self._buffer[total:]
            if message_type not in _KNOWN_TYPES:
                decoded.append(None)  # unknown type: skipped, forward compat
                continue
            decoded.append(_decode_payload(message_type, payload))
        return decoded


def _unpack(struct_type: struct.Struct, payload: bytes) -> tuple[Any, ...]:
    """Fixed-size unpack that maps struct.error to ProtocolError.

    A malformed payload length for a known type is a wire violation the peer
    must not be trusted past; leaking bare struct.error would let callers
    that catch ProtocolError miss it.
    """
    try:
        return struct_type.unpack(payload)
    except struct.error as error:
        raise ProtocolError(
            f"payload of {len(payload)} bytes does not fit the message type"
        ) from error


def _decode_payload(message_type: int, payload: bytes) -> Hello | Targets | Ping | Calib | State:
    if message_type == TYPE_HELLO:
        app_build, flags = _unpack(_HELLO, payload)
        return Hello(app_build, flags)
    if message_type == TYPE_TARGETS:
        try:
            frame_id, t_capture_ns, count = _TARGETS_HEAD.unpack_from(payload, 0)
        except struct.error as error:
            raise ProtocolError("truncated targets head") from error
        expected = _TARGETS_HEAD.size + count * _TARGET_BOX.size
        if len(payload) != expected:
            raise ProtocolError("targets payload length mismatch")
        boxes = []
        for index in range(count):
            offset = _TARGETS_HEAD.size + index * _TARGET_BOX.size
            class_id, confidence, cx, cy, width, height = _TARGET_BOX.unpack_from(payload, offset)
            boxes.append(TargetBox(class_id, confidence, cx, cy, width, height))
        return Targets(frame_id, t_capture_ns, tuple(boxes))
    if message_type in (TYPE_PING, TYPE_PONG):
        (t0_ns,) = _unpack(_PING, payload)
        return Ping(t0_ns, is_reply=message_type == TYPE_PONG)
    if message_type == TYPE_CALIB:
        values = _unpack(_CALIB, payload)
        flat, t_ns, rms = values[:9], values[9], values[10]
        matrix = tuple(
            (float(flat[row * 3]), float(flat[row * 3 + 1]), float(flat[row * 3 + 2]))
            for row in range(3)
        )
        return Calib(matrix, int(t_ns), float(rms))
    if message_type == TYPE_STATE:
        fields = _unpack(_STATE, payload)
        return State(*fields)
    raise ProtocolError(f"unknown message type {message_type}")
