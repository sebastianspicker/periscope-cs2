"""Shared-memory ring sink — contours out to another process on device 2.

The compositor (the fullscreen presenter, an external renderer, or a recording
tool) can live in a second process. A named shared-memory ring carries the
rendered frames across without touching the filesystem, so there is nothing to
poll, nothing to lock, and nothing left behind when the producer dies.

Wire format (little-endian, packed — no padding), HEADER_FORMAT:

    magic u32 "CV1S", version u32, width u32, height u32, stride u32,
    channels u32, fps_hint u32, frame_index u64, timestamp_ns u64, state u32

That is 7 u32, then 2 u64, then 1 u32 — 48 bytes total (frame_index at
offset 28, timestamp_ns at 36, state at 44). It is followed by
``slots`` frames of ``stride * height`` bytes each. The producer never waits:
``send`` writes the slot frame_index would land on and flips the header to
ready. Consumers compare the frame index before and after a copy to detect a
torn read. Slot 0 starts right after the header.
"""

from __future__ import annotations

import contextlib
import struct
import time
from dataclasses import dataclass
from typing import Any

import numpy as np

HEADER_FORMAT = "<7I2QI"  # magic, ver, w, h, stride, ch, fps | idx, ts | state
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
INDEX_OFFSET = struct.calcsize("<7I")  # packed: frame_index sits at 28, not 32
assert HEADER_SIZE == 48, "header layout changed (packed: idx 28, ts 36, state 44)"
MAGIC = 0x4356_3153  # big-endian spelling of "CV1S"; wire bytes read "S1VC"
# (little-endian). Pre-existing wire contract, guarded by
# test_contour_pipeline.py::test_ring_header_wire_contract — header_to_json
# derives its display text with the big-endian spelling so docs, mirror and
# constant agree on the *name* while the wire bytes stay untouched.
VERSION = 1

STATE_IDLE = 0
STATE_READY = 1
STATE_GONE = 2

KNOWN_PROTOCOL_VERSIONS = frozenset({VERSION})


class ProtocolVersionError(ValueError):
    """The wire contract version is not supported by this reader/writer.

    Raised on the clean-rejection path of ledger V1: a reader that does not
    speak a producer's version must fail with this error (never decode
    garbage). Add new versions to ``KNOWN_PROTOCOL_VERSIONS`` together with a
    migration note in the module docstring; old readers keep rejecting
    unknown versions loudly instead of silently misreading bytes.
    """


def _open_shm(name: str, size: int) -> Any:
    """Open a POSIX shared-memory segment, creating it when absent."""
    import multiprocessing.shared_memory as shared_memory

    return shared_memory.SharedMemory(name=name, create=True, size=size)


@dataclass(frozen=True)
class SharedFrameHeader:
    """Decoded view of the ring header."""

    width: int
    height: int
    stride: int
    channels: int
    fps_hint: int
    frame_index: int
    timestamp_ns: int
    state: int


class SharedFrameSink:
    """Producer side: publish rendered RGBA frames into a named ring."""

    def __init__(
        self,
        name: str,
        width: int,
        height: int,
        *,
        slots: int = 3,
        fps_hint: float = 60.0,
    ) -> None:
        if width <= 0 or height <= 0:
            raise ValueError("frame size must be positive")
        if slots < 1:
            raise ValueError("ring needs at least one slot")
        self._name = name
        self._width = int(width)
        self._height = int(height)
        self._stride = int(width) * 4
        self._slots = int(slots)
        self._fps_hint = int(max(1.0, fps_hint))
        self._closed = False
        size = HEADER_SIZE + self._slots * self._stride * height
        self._shm = _open_shm(name, size)
        # Zero the header so a consumer attaching before the first frame sees
        # idle state rather than garbage.
        self._publish(frame_index=0, timestamp_ns=0, state=STATE_IDLE)

    @property
    def frame_bytes(self) -> int:
        return self._stride * self._height

    @property
    def slots(self) -> int:
        return self._slots

    def _publish(self, *, frame_index: int, timestamp_ns: int, state: int) -> None:
        struct.pack_into(
            HEADER_FORMAT,
            self._shm.buf,
            0,
            MAGIC,
            VERSION,
            self._width,
            self._height,
            self._stride,
            4,
            self._fps_hint,
            frame_index,
            timestamp_ns,
            state,
        )

    def send(self, frame: np.ndarray, frame_index: int) -> None:
        """Publish one RGBA frame. Never blocks; the oldest slot is recycled.

        Transfer contract (ledger E32): exactly **one** bulk copy per frame —
        the payload is made contiguous once and written into its slot in a
        single slice assignment; there are no per-element or per-channel
        transfers. ``frame_index`` is the pipeline's 0-based frame counter;
        the ring's 1-based publish sequence is derived from it, so slot
        arithmetic is stable across producer restarts that replay the same
        index. Read-only input frames (the pipeline's rendered-snapshot
        contract, ledger E8) are accepted.
        """
        if self._closed:
            return
        if frame.ndim != 3 or frame.shape[2] != 4:
            raise ValueError("shared sink expects RGBA frames of shape (H, W, 4)")
        h, w = frame.shape[:2]
        if (w, h) != (self._width, self._height):
            raise ValueError(f"frame {w}x{h} does not match the ring {self._width}x{self._height}")
        contiguous = np.ascontiguousarray(frame, dtype=np.uint8)
        publish = int(frame_index) + 1
        slot = publish % self._slots
        offset = HEADER_SIZE + slot * self.frame_bytes
        self._shm.buf[offset : offset + contiguous.nbytes] = contiguous.ravel()
        self._publish(
            frame_index=publish,
            timestamp_ns=time.time_ns(),
            state=STATE_READY,
        )

    def close(self) -> None:
        """Mark the ring gone and release this process's mapping."""
        if self._closed:
            return
        self._closed = True
        if self._shm.buf is not None and len(self._shm.buf) >= HEADER_SIZE:
            struct.pack_into("<I", self._shm.buf, HEADER_SIZE - 4, STATE_GONE)
        self._shm.close()
        with contextlib.suppress(FileNotFoundError):
            self._shm.unlink()

    def __enter__(self) -> SharedFrameSink:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()


def negotiate_protocol(version: int) -> int:
    """Returns the protocol version to speak, or raises.

    Both sides call this with the version they found on the wire (readers)
    or intend to write (producers). Unknown versions are a loud, typed
    failure — the A3 incident class was silent byte-level misreads.
    """
    if version in KNOWN_PROTOCOL_VERSIONS:
        return version
    raise ProtocolVersionError(
        f"ring protocol version {version} unsupported (known: {sorted(KNOWN_PROTOCOL_VERSIONS)})"
    )


def header_to_json(header: SharedFrameHeader) -> str:
    """Canonical JSON mirror of a decoded header (tooling/test contract).

    Fixed field order, integer types only — the mirror must be byte-stable
    for golden comparisons and must never contain frame payload bytes. The
    magic text is derived from the wire constant so the two cannot drift.
    """
    magic_text = struct.pack(">I", MAGIC).decode("ascii")
    return (
        f'{{"magic":"{magic_text}","version":{VERSION},"width":{header.width},'
        f'"height":{header.height},"stride":{header.stride},'
        f'"channels":{header.channels},"fps_hint":{header.fps_hint},'
        f'"frame_index":{header.frame_index},'
        f'"timestamp_ns":{header.timestamp_ns},"state":{header.state}}}'
    )


def read_shared_header(shm: Any) -> SharedFrameHeader:
    """Decode the ring header from an attached shared-memory mapping."""
    values = struct.unpack_from(HEADER_FORMAT, shm.buf, 0)
    magic, version, width, height, stride, channels, fps, frame_index, ts, state = values
    if magic != MAGIC:
        raise ValueError("not a shared contour ring (magic mismatch)")
    negotiate_protocol(version)  # raises ProtocolVersionError on mismatch
    return SharedFrameHeader(
        width=width,
        height=height,
        stride=stride,
        channels=channels,
        fps_hint=fps,
        frame_index=frame_index,
        timestamp_ns=ts,
        state=state,
    )


def read_shared_frame(shm: Any, width: int, height: int, slots: int) -> np.ndarray:
    """Copy out the newest published frame as an RGBA array.

    The frame index is re-read after the copy: if the producer published a
    newer frame in between, the slot was rewritten mid-copy and the read is
    retried rather than returning a torn frame. Pass ``slots`` matching the
    producer's ring so slot arithmetic agrees on both sides.
    """
    if slots < 1:
        raise ValueError("slots must be positive")
    if width <= 0 or height <= 0:
        raise ValueError("frame size must be positive")
    stride = width * 4
    frame_bytes = stride * height
    while True:
        before = struct.unpack_from("<Q", shm.buf, INDEX_OFFSET)[0]
        if before == 0:
            raise RuntimeError("ring has no published frames yet")
        slot = int(before) % slots
        base = HEADER_SIZE + slot * frame_bytes
        frame = (
            np.frombuffer(shm.buf, dtype=np.uint8, count=frame_bytes, offset=base)
            .reshape(height, width, 4)
            .copy()
        )
        after = struct.unpack_from("<Q", shm.buf, INDEX_OFFSET)[0]
        if after == before:
            return frame
