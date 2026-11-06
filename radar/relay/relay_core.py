#!/usr/bin/env python3
"""relay_core.py — R6 public-radar relay: pure policy core.

Adapted from the research ledger (R6, docs/ADAPTATION_LEDGER.md):
tiansongyu/cs2_cheat `radar-relay` (MIT) supplies the room/token/limiter
design; avitran0/deadlocked `server/` supplies the shareable session UUID.
Pure logic only — sockets live in relay_server.py.

Policy decisions carried over from upstream:
- Direction of trust: the producer dials OUTBOUND; the relay never connects
  to the gaming machine.
- Producer and Viewer credentials are separate; a viewer can never publish.
- Per room, only the LATEST frame is retained (no history, no persistence —
  a deliberate privacy decision).
- Limits: 512 KiB frame cap, per-connection frame rate limit, per-room
  viewer cap.
- Producer reconnect backoff: capped exponential, reset only after the
  connection stayed stable for a grace period.

Self-test (no sockets, no network):
  python3 radar/relay/relay_core.py --self-test
"""

from __future__ import annotations

import argparse
import secrets
import sys
import time
from dataclasses import dataclass, field

MAX_FRAME_BYTES = 512 * 1024
DEFAULT_VIEWER_RATE_HZ = 40  # generous; the producer side rate-limits harder
PRODUCER_RATE_HZ = 30
PRODUCER_BURST = 60
DEFAULT_VIEWER_CAP = 16


class RelayError(Exception):
    """Rejected at the relay boundary (bad token, bad room, oversize...)."""


@dataclass
class RoomPolicy:
    """One room's credentials and limits."""

    room_id: str
    producer_token: str
    viewer_token: str
    viewer_rate_hz: int = DEFAULT_VIEWER_RATE_HZ
    producer_rate_hz: int = PRODUCER_RATE_HZ
    viewer_cap: int = DEFAULT_VIEWER_CAP


def new_session_token() -> str:
    """Short-lived browser session token (deadlocked session-UUID pattern)."""
    return secrets.token_urlsafe(16)


class TokenAuth:
    """Role-scoped bearer tokens per room.

    Producer and viewer tokens are independent secrets; a viewer token must
    never be accepted on the publish path and vice versa.
    """

    def __init__(self, rooms: list[RoomPolicy]) -> None:
        self._rooms = {room.room_id: room for room in rooms}

    def room(self, room_id: str) -> RoomPolicy:
        room = self._rooms.get(room_id)
        if room is None:
            raise RelayError(f"unknown room: {room_id}")
        return room

    def authenticate(self, room_id: str, token: str, role: str) -> RoomPolicy:
        room = self.room(room_id)
        expected = room.producer_token if role == "producer" else room.viewer_token
        if not token or not secrets.compare_digest(token, expected):
            raise RelayError(f"{role} authentication failed for room {room_id}")
        return room


class LatestFrameStore:
    """Per-room bounded latest-frame cell (no queue, no history)."""

    def __init__(self) -> None:
        self._frames: dict[str, tuple[int, bytes]] = {}

    def publish(self, room_id: str, sequence: int, frame: bytes) -> None:
        if len(frame) > MAX_FRAME_BYTES:
            raise RelayError(
                f"frame of {len(frame)} bytes exceeds the {MAX_FRAME_BYTES} cap"
            )
        self._frames[room_id] = (sequence, frame)

    def latest(self, room_id: str) -> tuple[int, bytes] | None:
        return self._frames.get(room_id)

    def drop(self, room_id: str) -> None:
        self._frames.pop(room_id, None)


class FrameRateLimiter:
    """Token bucket per connection (refill on demand, no background task)."""

    def __init__(self, rate_hz: float, burst: int) -> None:
        self.rate_hz = float(rate_hz)
        self.burst = float(burst)
        self._tokens = float(burst)
        self._last = time.monotonic()

    def allow(self) -> bool:
        now = time.monotonic()
        self._tokens = min(
            self.burst, self._tokens + (now - self._last) * self.rate_hz
        )
        self._last = now
        if self._tokens >= 1.0:
            self._tokens -= 1.0
            return True
        return False


class ProducerBackoff:
    """Producer reconnect backoff: capped exponential, reset only after the
    connection stayed stable for `stable_s` seconds (upstream bug lesson:
    resetting backoff on every successful dial turns a flapping link into a
    hot reconnect loop)."""

    def __init__(
        self,
        base_s: float = 0.5,
        factor: float = 2.0,
        max_s: float = 30.0,
        stable_s: float = 10.0,
    ) -> None:
        self.base_s = base_s
        self.factor = factor
        self.max_s = max_s
        self.stable_s = stable_s
        self._delay = base_s
        self._stable_since: float | None = None

    def connection_opened(self, now: float) -> None:
        self._stable_since = now

    def connection_closed(self, now: float) -> float:
        """Call after a close; returns how long to wait before redialing."""
        stable = self._stable_since
        self._stable_since = None
        if stable is not None and now - stable >= self.stable_s:
            self._delay = self.base_s  # the link proved itself; reset
        else:
            self._delay = min(self._delay * self.factor, self.max_s)
        return self._delay

    @property
    def delay(self) -> float:
        return self._delay


@dataclass
class RoomState:
    policy: RoomPolicy
    viewers: set[str] = field(default_factory=set)
    producer_id: str | None = None


class RoomRegistry:
    """Viewer caps and producer exclusivity per room."""

    def __init__(self, auth: TokenAuth) -> None:
        self._auth = auth
        self._states: dict[str, RoomState] = {}

    def _state(self, room_id: str) -> RoomState:
        state = self._states.get(room_id)
        if state is None:
            state = RoomState(self._auth.room(room_id))
            self._states[room_id] = state
        return state

    def producer_join(self, room_id: str, connection_id: str) -> None:
        state = self._state(room_id)
        if state.producer_id not in (None, connection_id):
            raise RelayError(f"room {room_id} already has a producer")
        state.producer_id = connection_id

    def producer_leave(self, room_id: str, connection_id: str) -> None:
        state = self._states.get(room_id)
        if state is not None and state.producer_id == connection_id:
            state.producer_id = None

    def viewer_join(self, room_id: str, connection_id: str) -> None:
        state = self._state(room_id)
        if connection_id in state.viewers:
            return
        if len(state.viewers) >= state.policy.viewer_cap:
            raise RelayError(f"room {room_id} viewer cap reached")
        state.viewers.add(connection_id)

    def viewer_leave(self, room_id: str, connection_id: str) -> None:
        state = self._states.get(room_id)
        if state is not None:
            state.viewers.discard(connection_id)

    def viewer_count(self, room_id: str) -> int:
        state = self._states.get(room_id)
        return len(state.viewers) if state else 0


def self_test() -> int:
    """Exercises room isolation, token scoping, caps, limits and backoff."""
    failures = 0

    def check(ok: bool, what: str) -> None:
        nonlocal failures
        if not ok:
            print(f"FAIL {what}")
            failures += 1

    rooms = [
        RoomPolicy("alpha", "p-token-alpha", "v-token-alpha"),
        RoomPolicy("beta", "p-token-beta", "v-token-beta"),
    ]
    auth = TokenAuth(rooms)

    # Role scoping: a viewer token cannot publish and vice versa.
    try:
        auth.authenticate("alpha", "v-token-alpha", "producer")
        check(False, "viewer token rejected on publish path")
    except RelayError:
        check(True, "viewer token rejected on publish path")
    try:
        auth.authenticate("alpha", "p-token-alpha", "viewer")
        check(False, "producer token rejected on viewer path")
    except RelayError:
        check(True, "producer token rejected on viewer path")
    try:
        auth.authenticate("alpha", "p-token-beta", "producer")
        check(False, "cross-room token rejected")
    except RelayError:
        check(True, "cross-room token rejected")
    try:
        auth.authenticate("gamma", "p-token-alpha", "producer")
        check(False, "unknown room rejected")
    except RelayError:
        check(True, "unknown room rejected")

    # Frame cap.
    store = LatestFrameStore()
    try:
        store.publish("alpha", 1, b"x" * (MAX_FRAME_BYTES + 1))
        check(False, "oversize frame rejected")
    except RelayError:
        check(True, "oversize frame rejected")
    store.publish("alpha", 1, b"frame-1")
    store.publish("alpha", 2, b"frame-2")
    latest = store.latest("alpha")
    check(latest is not None and latest[0] == 2 and latest[1] == b"frame-2",
          "only the latest frame is retained")

    # Rate limiter: burst honored, sustained rate bounded.
    limiter = FrameRateLimiter(rate_hz=1000.0, burst=3)
    check(all(limiter.allow() for _ in range(3)), "burst of three allowed")
    check(not limiter.allow(), "fourth immediate frame limited")

    # Producer backoff: flapping link grows, stable link resets.
    backoff = ProducerBackoff(base_s=0.5, factor=2.0, max_s=30.0, stable_s=10.0)
    t = 0.0
    for expected in (1.0, 2.0, 4.0):
        backoff.connection_opened(t)
        t += 0.1  # flap: opened, closed quickly
        delay = backoff.connection_closed(t)
        t += delay
        check(abs(delay - expected) < 1e-9, f"flap delay {expected}")
    # A long-lived connection resets backoff to base.
    backoff.connection_opened(t)
    delay = backoff.connection_closed(t + 11.0)
    check(abs(delay - 0.5) < 1e-9, "stable connection resets backoff")

    # Viewer cap.
    registry = RoomRegistry(auth)
    registry.producer_join("alpha", "conn-p")
    for i in range(DEFAULT_VIEWER_CAP):
        registry.viewer_join("alpha", f"conn-v{i}")
    try:
        registry.viewer_join("alpha", "conn-overflow")
        check(False, "viewer cap enforced")
    except RelayError:
        check(True, "viewer cap enforced")
    registry.viewer_leave("alpha", "conn-v0")
    registry.viewer_join("alpha", "conn-overflow")
    check(registry.viewer_count("alpha") == DEFAULT_VIEWER_CAP,
          "cap after leave/rejoin")

    # Session tokens are unique enough for share links.
    check(new_session_token() != new_session_token(), "session tokens differ")

    if failures != 0:
        print(f"{failures} relay core check(s) failed")
        return 1
    print("relay_core: all checks passed")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    parser.print_usage()
    return 2


if __name__ == "__main__":
    sys.exit(main())
