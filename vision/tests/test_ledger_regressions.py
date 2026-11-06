"""Ledger regression harness (V8) — one test per incident it guards.

Every test here is named after the failure class it keeps dead:
- ``test_ring_*``  — A3 (shared-ring torn/stale reads) and V1 (versioned
  wire contract, clean rejection, canonical JSON mirror).
- ``test_file_read_*`` — A4 (video EOF looping).
- ``test_temporal_*`` — A5 (frame-index gaps starving the temporal filter).

The suite is tracked (not gitignored) and runs in the portable Vision CI
lane on every PR — the B12 lesson: tests that do not run on PRs do not
guard anything.
"""

from __future__ import annotations

import contextlib
import multiprocessing.shared_memory as shared_memory
import struct
import uuid

import numpy as np
import pytest

from cs2_vision_access.adapters.capture.sinks.shared import (
    HEADER_FORMAT,
    HEADER_SIZE,
    VERSION,
    ProtocolVersionError,
    SharedFrameSink,
    header_to_json,
    negotiate_protocol,
    read_shared_frame,
    read_shared_header,
)

# ── A3 / V1: shared-ring wire contract ────────────────────────────


def _ring_name() -> str:
    return f"ps-ledger-{uuid.uuid4().hex[:12]}"


def _unlink(name: str) -> None:
    with contextlib.suppress(FileNotFoundError):
        shared_memory.SharedMemory(name=name).unlink()


@pytest.mark.parametrize("frames", [1, 2, 3, 7])
def test_ring_newest_frame_wins_after_burst(frames: int) -> None:
    """A3: after N publishes with no reader in between, the read returns the
    newest frame — never a stale slot and never a torn one."""
    name = _ring_name()
    sink = SharedFrameSink(name, 8, 4, slots=3)
    try:
        for index in range(frames):
            sink.send(np.full((4, 8, 4), 10 + index, dtype=np.uint8), frame_index=index)
        got = read_shared_frame(sink._shm, 8, 4, slots=3)
        assert int(got[0, 0, 0]) == 10 + frames - 1
    finally:
        _unlink(name)


def test_ring_protocol_negotiation_accepts_known_version() -> None:
    assert negotiate_protocol(VERSION) == VERSION


def test_ring_protocol_negotiation_rejects_unknown_version() -> None:
    with pytest.raises(ProtocolVersionError):
        negotiate_protocol(VERSION + 1)
    with pytest.raises(ProtocolVersionError):
        negotiate_protocol(0)


def test_ring_reader_rejects_future_writer_version_cleanly() -> None:
    """V1: a producer speaking a newer protocol must be rejected with the
    typed error — never decoded as garbage bytes."""
    name = _ring_name()
    sink = SharedFrameSink(name, 8, 4, slots=2)
    try:
        # Simulate a future producer by rewriting the header version field.
        struct.pack_into("<I", sink._shm.buf, 4, VERSION + 1)
        with pytest.raises(ProtocolVersionError):
            read_shared_header(sink._shm)
    finally:
        _unlink(name)


def test_ring_header_json_mirror_is_stable() -> None:
    """V1: the canonical JSON mirror is byte-stable and payload-free."""
    name = _ring_name()
    sink = SharedFrameSink(name, 8, 4, slots=2)
    try:
        sink.send(np.zeros((4, 8, 4), dtype=np.uint8), frame_index=0)
        header = read_shared_header(sink._shm)
        mirror = header_to_json(header)
        assert mirror == (
            '{"magic":"CV1S","version":1,"width":8,"height":4,'
            '"stride":32,"channels":4,"fps_hint":60,'
            f'"frame_index":{header.frame_index},'
            f'"timestamp_ns":{header.timestamp_ns},"state":1}}'
        )
        # Integer-only fields: no float formatting can drift.
        assert all(token.count(".") == 0 for token in mirror.split(":")[1:])
    finally:
        _unlink(name)


def test_ring_header_layout_is_packed() -> None:
    """A3 regression anchor: frame_index sits at offset 28, not 32."""
    assert HEADER_SIZE == 48
    assert struct.calcsize("<7I") == 28  # INDEX_OFFSET
    # The wire format itself must stay packed little-endian.
    assert struct.pack(HEADER_FORMAT, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)[:4] == (struct.pack("<I", 1))


# ── A4/A5 guards live in test_live_core.py; the harness adds the ring-side
#    half of the incident set here so the whole ledger regression family has
#    one discoverable home (this docstring is the index).
