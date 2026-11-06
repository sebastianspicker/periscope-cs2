"""Ledger L6 + L7 tests: USB transport protocol and latency compensation."""

from __future__ import annotations

import struct

import pytest

from periscope_lens.latency import (
    LatencyError,
    RoundTrip,
    VelocityPredictor,
    breakdown,
)
from periscope_lens.transport import (
    MAX_FRAME_BYTES,
    PROTOCOL_VERSION,
    Calib,
    Decoder,
    Hello,
    Ping,
    ProtocolError,
    State,
    TargetBox,
    Targets,
    encode,
)

# ── L6: transport ─────────────────────────────────────────────────


def test_round_trip_all_message_types() -> None:
    decoder = Decoder()
    originals: list[object] = [
        Hello(app_build=7, flags=3),
        Targets(
            frame_id=2**53,  # u64-safe magnitude
            t_capture_ns=1762345678901234567,
            boxes=(
                TargetBox(0, 0.5, 640.0, 360.0, 48.0, 96.0),
                TargetBox(1, 0.75, 900.5, 400.25, 40.0, 40.0),
            ),
        ),
        Ping(t0_ns=123456789, is_reply=False),
        Ping(t0_ns=123456789, is_reply=True),
        Calib(matrix=((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)), t_ns=42, rms=0.25),
        State(fps_measured=58, fps_target=60, roi_x=320, roi_y=180, roi_w=640, roi_h=360, flags=1),
    ]
    for message in originals:
        decoder.feed(encode(message))  # type: ignore[arg-type]
    decoded = decoder.messages()
    assert decoded == originals


def test_unknown_version_rejected_cleanly() -> None:
    bad = bytes([PROTOCOL_VERSION + 1, 1]) + struct.pack("<I", 0)
    decoder = Decoder()
    decoder.feed(bad)
    with pytest.raises(ProtocolError, match="unsupported protocol version"):
        decoder.messages()


def test_unknown_type_skipped_with_payload() -> None:
    payload = b"future-data"
    frame = bytes([PROTOCOL_VERSION, 99]) + struct.pack("<I", len(payload))
    decoder = Decoder()
    decoder.feed(frame + payload)
    decoder.feed(encode(Hello(app_build=1, flags=0)))
    messages = decoder.messages()
    assert messages == [None, Hello(1, 0)]


def test_truncated_frame_waits_and_oversized_rejected() -> None:
    decoder = Decoder()
    frame = encode(Hello(app_build=5, flags=0))
    decoder.feed(frame[:3])
    assert decoder.messages() == []  # waits for the rest
    decoder.feed(frame[3:])
    assert decoder.messages() == [Hello(5, 0)]

    oversize = bytes([PROTOCOL_VERSION, 1]) + struct.pack("<I", MAX_FRAME_BYTES + 1)
    decoder.feed(oversize)
    with pytest.raises(ProtocolError, match="exceeds cap"):
        decoder.messages()


def test_targets_payload_length_mismatch_rejected() -> None:
    head = struct.pack("<QQB", 1, 2, 2)  # promises 2 boxes
    one_box = struct.pack("<Bfffff", 0, 0.9, 1.0, 2.0, 3.0, 4.0)
    payload = head + one_box
    frame = bytes([PROTOCOL_VERSION, 2]) + struct.pack("<I", len(payload)) + payload
    decoder = Decoder()
    decoder.feed(frame)
    with pytest.raises(ProtocolError, match="length mismatch"):
        decoder.messages()


def test_malformed_fixed_size_payloads_raise_protocol_error() -> None:
    """Wrong payload length for a known type must be ProtocolError, not a
    stray struct.error callers can miss."""
    for message_type, payload in ((1, b"\x01"), (3, b""), (5, b"\x00" * 4), (6, b"\x00")):
        frame = bytes([PROTOCOL_VERSION, message_type]) + struct.pack("<I", len(payload)) + payload
        decoder = Decoder()
        decoder.feed(frame)
        with pytest.raises(ProtocolError):
            decoder.messages()


def test_encode_rejects_more_boxes_than_u8_count() -> None:
    too_many = tuple(TargetBox(0, 0.5, 1.0, 2.0, 3.0, 4.0) for _ in range(256))
    with pytest.raises(ProtocolError, match="u8 count"):
        encode(Targets(frame_id=1, t_capture_ns=2, boxes=too_many))
    # 255 is the boundary that must still fit.
    encode(Targets(frame_id=1, t_capture_ns=2, boxes=too_many[:255]))


# ── L7: latency ───────────────────────────────────────────────────


def test_round_trip_tracks_and_rejects_outliers() -> None:
    rtt = RoundTrip()
    base_ns = 1_000_000_000
    for offset_ms in (20, 22, 21, 19, 23):
        assert rtt.record(base_ns, base_ns + offset_ms * 1_000_000)
    assert rtt.measured and 19.0 < rtt.rtt_ms < 23.0
    # A scheduler hiccup (5× the median) is rejected, not averaged in.
    assert not rtt.record(base_ns, base_ns + 500_000_000)
    assert rtt.rtt_ms < 30.0
    # Negative deltas are never samples.
    assert not rtt.record(10, 5)
    empty = RoundTrip()
    assert not empty.measured and empty.rtt_ms == 0.0


def test_sustained_latency_step_is_adopted() -> None:
    """A regime change (20 ms → 200 ms transport) must not be rejected
    forever by the stale median; two consecutive outliers adopt it."""
    rtt = RoundTrip()
    base = 1_000_000_000
    for _ in range(5):
        assert rtt.record(base, base + 20_000_000)
    # Single spike: rejected, old regime kept (reject streak = 1).
    assert not rtt.record(base, base + 500_000_000)
    assert rtt.rtt_ms < 30.0
    # A second consecutive outlier is the regime change: adopted, and the
    # stale samples are dropped with the old regime.
    assert rtt.record(base, base + 200_000_000)
    assert rtt.rtt_ms == pytest.approx(200.0)
    # The stale 20 ms samples were dropped with the old regime.
    assert len(rtt._samples_ms) == 1
    # And steady-state recording continues normally afterwards.
    assert rtt.record(base, base + 210_000_000)
    assert 200.0 <= rtt.rtt_ms <= 210.0


def test_velocity_predictor_tracks_and_extrapolates() -> None:
    predictor = VelocityPredictor()
    # A target moving +100 px/s on x, stationary y, sampled at 60 fps.
    for index in range(1, 61):
        predictor.observe(index / 60.0 * 100.0, 0.0, dt_s=1 / 60.0)
    predicted = predictor.extrapolate(80.0)  # 80 ms ahead
    assert predicted[0] == pytest.approx((60.0 / 60.0) * 100.0 + 100.0 * 0.08, abs=2.0)
    assert predicted[1] == pytest.approx(0.0, abs=2.0)
    with pytest.raises(LatencyError):
        predictor.extrapolate(-1.0)


def test_breakdown_prefers_measured_over_assumed() -> None:
    rtt = RoundTrip()
    rtt.record(0, 40_000_000)  # 40 ms RTT → 20 ms transport segment
    full = breakdown(camera_to_detect_ms=25.0, rtt=rtt, assumed_total_ms=80.0)
    assert full.capture_to_detect_measured and full.transport_measured
    assert full.compensation_ms == pytest.approx(25.0 + 20.0)

    nothing = breakdown(camera_to_detect_ms=None, rtt=RoundTrip(), assumed_total_ms=80.0)
    assert not nothing.capture_to_detect_measured
    assert not nothing.transport_measured
    assert nothing.compensation_ms == 80.0, (
        "never zero by omission: fall back to the assumed budget"
    )

    partial = breakdown(camera_to_detect_ms=25.0, rtt=RoundTrip())
    assert partial.compensation_ms == 80.0  # one measured segment alone
    # does not validate the whole budget

    with pytest.raises(LatencyError):
        breakdown(camera_to_detect_ms=-1.0, rtt=RoundTrip())


def test_encode_rejects_out_of_range_fields() -> None:
    cases = [
        Hello(app_build=0x10000, flags=0),
        Hello(app_build=0, flags=0x1_0000_0000),
        Ping(t0_ns=-1, is_reply=False),
        State(0x10000, 60, 0, 0, 0, 0, 0),
        State(60, 60, 0, 0, 0, 0, 0x100),
        Calib(matrix=((1.0, 0.0, 0.0), (0.0, 1.0, 0.0)), t_ns=0, rms=0.0),
    ]
    for message in cases:
        with pytest.raises(ProtocolError):
            encode(message)  # type: ignore[arg-type]


def test_encode_rejects_out_of_range_targets() -> None:
    box = TargetBox(0, 0.5, 1.0, 2.0, 3.0, 4.0)
    with pytest.raises(ProtocolError, match="class_id"):
        encode(Targets(1, 2, (TargetBox(256, 0.5, 1.0, 2.0, 3.0, 4.0),)))
    with pytest.raises(ProtocolError, match="frame_id"):
        encode(Targets(-1, 2, (box,)))
