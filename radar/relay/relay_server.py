#!/usr/bin/env python3
"""relay_server.py: R6 public-radar relay server (stdlib asyncio + RFC6455).

Wires the pure policy core (relay_core.py) to sockets:

  producer (game PC, outbound wss://)          viewer (browser)
        |  WS /api/v1/publish                        ^
        |  Authorization: Bearer <producer token>    |  WS /api/v1/stream
        v                                            |  ?room=..&token=..
  Caddy:443 (TLS, WS proxy) -> relay_server         |
                                                     + session minting:
                                     POST /api/v1/session (producer token)

The relay never connects to the gaming machine; viewers only ever see the
latest published frame; nothing is persisted.

The socket layer is deliberately thin: framing, routing, auth calls. All
policy (tokens, rooms, caps, limits, frame cap, latest-frame semantics,
backoff) lives in relay_core.py and is unit-tested there.

Self-test (loopback integration, no external network):
  python3 radar/relay/relay_server.py --self-test
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import json
import secrets
import sys
import time
import urllib.parse
from typing import Any

from relay_core import (
    DEFAULT_VIEWER_CAP,
    MAX_FRAME_BYTES,
    PRODUCER_BURST,
    FrameRateLimiter,
    LatestFrameStore,
    RelayError,
    RoomPolicy,
    RoomRegistry,
    TokenAuth,
    new_session_token,
)

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
SESSION_TTL_S = 3600
PRODUCER_VIOLATION_LIMIT = 10


def ws_accept(key: str) -> str:
    digest = hashlib.sha1((key + WS_GUID).encode("ascii")).digest()
    return base64.b64encode(digest).decode("ascii")


def build_frame(opcode: int, payload: bytes, mask: bytes | None = None) -> bytes:
    header = bytes([(0x80 | opcode) & 0xFF])
    length = len(payload)
    if length < 126:
        header += bytes([(0x80 if mask else 0) | length])
    elif length < 1 << 16:
        header += bytes([(0x80 if mask else 0) | 126]) + length.to_bytes(2, "big")
    else:
        header += bytes([(0x80 if mask else 0) | 127]) + length.to_bytes(8, "big")
    if mask is not None:
        header += mask
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        return header + masked
    return header + payload


async def read_frame(reader: asyncio.StreamReader) -> tuple[int, bytes]:
    head = await reader.readexactly(2)
    opcode = head[0] & 0x0F
    masked = bool(head[1] & 0x80)
    length = head[1] & 0x7F
    if length == 126:
        length = int.from_bytes(await reader.readexactly(2), "big")
    elif length == 127:
        length = int.from_bytes(await reader.readexactly(8), "big")
    # Reject before allocating: a peer can otherwise claim an absurd length
    # and make the server buffer it.
    if length > MAX_FRAME_BYTES:
        raise RelayError(f"frame of {length} bytes exceeds cap")
    mask = await reader.readexactly(4) if masked else None
    payload = await reader.readexactly(length) if length else b""
    if mask is not None:
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    return opcode, payload


class RelayServer:
    def __init__(self, rooms: list[RoomPolicy]) -> None:
        self.auth = TokenAuth(rooms)
        self.registry = RoomRegistry(self.auth)
        self.store = LatestFrameStore()
        self._sessions: dict[str, tuple[str, float]] = {}
        self._viewers: dict[str, dict[str, tuple[asyncio.StreamWriter,
                                                 FrameRateLimiter]]] = {}
        self._sequences: dict[str, int] = {}
        # Producer rate limiting is per connection (built from the room's own
        # policy in _run_producer), a shared limiter would let one room's
        # producer starve another's.
        self._producer_violations: dict[str, int] = {}

    # ------------------------------------------------------------------ HTTP

    async def handle_session(self, headers: dict[str, str]) -> tuple[int, str]:
        room_id = headers.get("x-radar-room", "")
        token = headers.get("authorization", "").removeprefix("Bearer ").strip()
        try:
            self.auth.authenticate(room_id, token, "producer")
        except RelayError as error:
            return 401, json.dumps({"error": str(error)})
        session = new_session_token()
        self._sessions[session] = (room_id, time.monotonic() + SESSION_TTL_S)
        self._expire_sessions()
        body = json.dumps(
            {
                "session_token": session,
                "room": room_id,
                "viewer_url": f"/api/v1/stream?room={room_id}"
                f"&session={session}",
            }
        )
        return 200, body

    def _expire_sessions(self) -> None:
        now = time.monotonic()
        for token, (_, expiry) in list(self._sessions.items()):
            if expiry < now:
                del self._sessions[token]

    # ------------------------------------------------------------------- WS

    async def handle_websocket(
        self, path: str, headers: dict[str, str],
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        key = headers.get("sec-websocket-key")
        if not key:
            await self._http_response(writer, 400, "missing websocket key")
            return

        # Authenticate and claim the room role BEFORE completing the
        # handshake: a rejected peer gets a plain HTTP error, never a socket
        # that looks connected.
        connection_id: str | None = None
        claimed_room: str | None = None
        if path.startswith("/api/v1/publish"):
            room_id = headers.get("x-radar-room", "")
            token = headers.get("authorization", "").removeprefix(
                "Bearer").strip()
            connection_id = secrets.token_urlsafe(8)
            try:
                self.auth.authenticate(room_id, token, "producer")
                self.registry.producer_join(room_id, connection_id)
                claimed_room = room_id
            except RelayError as error:
                await self._http_response(writer, 401,
                                          json.dumps({"error": str(error)}))
                return
        elif path.startswith("/api/v1/stream"):
            ok, error = await self._authorize_viewer(path)
            if not ok:
                await self._http_response(writer, 401,
                                          json.dumps({"error": error}))
                return

        try:
            writer.write(
                b"HTTP/1.1 101 Switching Protocols\r\n"
                b"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                b"Sec-WebSocket-Accept: " + ws_accept(key).encode() + b"\r\n\r\n"
            )
            await writer.drain()
        except (ConnectionError, RuntimeError):
            # Handshake failed after the producer claim: release the room or
            # every future producer would be locked out by a ghost holder.
            if claimed_room is not None and connection_id is not None:
                self.registry.producer_leave(claimed_room, connection_id)
            writer.close()
            return

        if path.startswith("/api/v1/publish"):
            assert connection_id is not None
            await self._run_producer(headers, connection_id, reader, writer)
        elif path.startswith("/api/v1/stream"):
            await self._run_viewer(path, reader, writer)
        else:
            await self._close_ws(writer)

    async def _authorize_viewer(
        self, path: str
    ) -> tuple[bool, str]:
        """Pre-handshake viewer authorization (session token or viewer token).

        The room claim itself (viewer cap) happens in _run_viewer, which owns
        the per-connection id.
        """
        params = self._viewer_params(path)
        room_id = params.get("room", "")
        session = params.get("session", "")
        entry = self._sessions.get(session)
        if entry is not None:
            session_room, expiry = entry
            if expiry < time.monotonic():
                # Expired sessions are retired at use, not only at the next
                # mint, a quiet relay must not keep stale sessions alive.
                del self._sessions[session]
            elif session_room != room_id:
                return False, "session does not match room"
            else:
                return True, ""
        try:
            self.auth.authenticate(room_id, params.get("token", ""), "viewer")
        except RelayError as error:
            return False, str(error)
        return True, ""

    @staticmethod
    def _viewer_params(path: str) -> dict[str, str]:
        query = path.split("?", 1)[1] if "?" in path else ""
        params: dict[str, str] = {}
        for part in query.split("&"):
            if "=" in part:
                key, value = part.split("=", 1)
                params[urllib.parse.unquote(key)] = urllib.parse.unquote(value)
        return params

    async def _http_response(self, writer: asyncio.StreamWriter, code: int,
                             body: str) -> None:
        reason = {200: "OK", 400: "Bad Request", 401: "Unauthorized",
                  404: "Not Found", 413: "Payload Too Large"}.get(code, "?")
        payload = body.encode()
        writer.write(
            f"HTTP/1.1 {code} {reason}\r\nContent-Type: application/json\r\n"
            f"Content-Length: {len(payload)}\r\nConnection: close\r\n\r\n"
.encode() + payload
        )
        await writer.drain()

    async def _close_ws(self, writer: asyncio.StreamWriter) -> None:
        try:
            writer.write(build_frame(0x8, b""))
            await writer.drain()
        except (ConnectionError, RuntimeError):
            # The peer is already gone (ConnectionError) or the transport is
            # closing under us (RuntimeError), nothing to close politely.
            pass
        writer.close()

    async def _run_producer(self, headers: dict[str, str],
                            connection_id: str,
                            reader: asyncio.StreamReader,
                            writer: asyncio.StreamWriter) -> None:
        room_id = headers.get("x-radar-room", "")
        # Auth and room claim already happened pre-handshake.
        room_policy = self.auth.room(room_id)
        limiter = FrameRateLimiter(rate_hz=room_policy.producer_rate_hz,
                                   burst=PRODUCER_BURST)

        violations = 0
        try:
            while True:
                try:
                    opcode, payload = await read_frame(reader)
                except (asyncio.IncompleteReadError, ConnectionError, RelayError):
                    break
                if opcode == 0x8:  # close
                    break
                if opcode == 0x9:  # ping -> pong
                    writer.write(build_frame(0xA, payload))
                    await writer.drain()
                    continue
                if opcode not in (0x1, 0x2):
                    continue
                if len(payload) > MAX_FRAME_BYTES:
                    break  # frame cap: drop the producer connection
                if not limiter.allow():
                    violations += 1
                    self._producer_violations[connection_id] = violations
                    if violations >= PRODUCER_VIOLATION_LIMIT:
                        break
                    continue
                violations = 0
                self._sequences[room_id] = self._sequences.get(room_id, 0) + 1
                sequence = self._sequences[room_id]
                try:
                    self.store.publish(room_id, sequence, payload)
                except RelayError:
                    break
                await self._broadcast(room_id, sequence, payload)
        finally:
            self.registry.producer_leave(room_id, connection_id)
            self._producer_violations.pop(connection_id, None)
            await self._close_ws(writer)

    async def _run_viewer(self, path: str, reader: asyncio.StreamReader,
                          writer: asyncio.StreamWriter) -> None:
        params = self._viewer_params(path)
        room_id = params.get("room", "")
        connection_id = secrets.token_urlsafe(8)
        policy = self.auth.room(room_id)
        try:
            self.registry.viewer_join(room_id, connection_id)
        except RelayError:
            await self._close_ws(writer)
            return

        limiter = FrameRateLimiter(rate_hz=policy.viewer_rate_hz,
                                   burst=policy.viewer_rate_hz)
        self._viewers.setdefault(room_id, {})[connection_id] = (writer, limiter)
        try:
            # Latest frame on connect: a fresh viewer is immediately current.
            latest = self.store.latest(room_id)
            if latest is not None and limiter.allow():
                writer.write(build_frame(0x1, latest[1]))
                await writer.drain()
            while True:
                try:
                    opcode, _payload = await read_frame(reader)
                except (asyncio.IncompleteReadError, ConnectionError, RelayError):
                    break
                if opcode == 0x8:
                    break
                if opcode == 0x9:
                    writer.write(build_frame(0xA, b""))
                    await writer.drain()
        finally:
            viewers = self._viewers.get(room_id)
            if viewers is not None:
                viewers.pop(connection_id, None)
            self.registry.viewer_leave(room_id, connection_id)
            await self._close_ws(writer)

    async def _broadcast(self, room_id: str, sequence: int,
                         payload: bytes) -> None:
        viewers = self._viewers.get(room_id, {})
        for writer, limiter in list(viewers.values()):
            # Bounded-latest broadcasting: a rate-limited viewer skips this
            # frame and catches up on the next one (never queues).
            if not limiter.allow():
                continue
            try:
                writer.write(build_frame(0x1, payload))
                await writer.drain()
            except (ConnectionError, RuntimeError):
                continue

    # ---------------------------------------------------------------- server

    async def serve(self, host: str, port: int) -> tuple[int, Any]:
        async def on_connection(
            reader: asyncio.StreamReader, writer: asyncio.StreamWriter
        ) -> None:
            try:
                request_line = await asyncio.wait_for(
                    reader.readline(), timeout=10
                )
                if not request_line:
                    return
                parts = request_line.decode("latin-1").strip().split(" ")
                if len(parts) < 2:
                    return
                method, path = parts[0], parts[1]
                headers: dict[str, str] = {}
                for _ in range(100):  # bounded header count
                    line = await reader.readline()
                    if line in (b"\r\n", b"\n", b""):
                        break
                    name, _, value = line.decode("latin-1").partition(":")
                    headers[name.strip().lower()] = value.strip()

                if path.startswith("/api/v1/session") and method == "POST":
                    code, body = await self.handle_session(headers)
                    await self._http_response(writer, code, body)
                    writer.close()
                    return
                upgrade = headers.get("upgrade", "").lower()
                if upgrade == "websocket":
                    await self.handle_websocket(path, headers, reader, writer)
                    return
                await self._http_response(writer, 404, '{"error":"not found"}')
                writer.close()
            except (asyncio.IncompleteReadError, ConnectionError,
                    asyncio.TimeoutError, ValueError):
                # ValueError also covers the StreamReader header-line limit.
                writer.close()

        server = await asyncio.start_server(on_connection, host, port)
        port = server.sockets[0].getsockname()[1]  # type: ignore[union-attr]
        return port, server


# ------------------------------------------------------------------ self-test

async def _loopback_test() -> int:
    """Full loopback: auth failures, publish, latest-on-connect, caps."""
    failures = 0

    def check(ok: bool, what: str) -> None:
        nonlocal failures
        if not ok:
            print(f"FAIL {what}")
            failures += 1

    rooms = [RoomPolicy("alpha", "p-secret", "v-secret",
                        viewer_rate_hz=1000, viewer_cap=DEFAULT_VIEWER_CAP)]
    relay = RelayServer(rooms)
    port, server = await relay.serve("127.0.0.1", 0)

    async def _read_http(reader: asyncio.StreamReader) -> tuple[bytes, bytes]:
        status = await reader.readline()
        length = 0
        while True:
            line = await reader.readline()
            if line in (b"\r\n", b"\n", b""):
                break
            name, _, value = line.decode("latin-1").partition(":")
            if name.strip().lower() == "content-length":
                length = int(value.strip())
        body = await reader.readexactly(length) if length else b""
        return status, body

    async def connect(path: str, headers: dict[str, str] | None = None,
                      upgrade: bool = True) -> tuple[
            asyncio.StreamReader, asyncio.StreamWriter, bytes]:
        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        head = f"{'GET' if upgrade else 'POST'} {path} HTTP/1.1\r\n"
        head += "Host: localhost\r\n"
        if upgrade:
            key = base64.b64encode(secrets.token_bytes(16)).decode()
            head += (f"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                     f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n")
        for name, value in (headers or {}).items():
            head += f"{name}: {value}\r\n"
        head += "\r\n"
        writer.write(head.encode())
        await writer.drain()
        status, _body = await _read_http(reader)
        return reader, writer, status

    async def ws_send(writer: asyncio.StreamWriter, payload: bytes) -> None:
        mask = secrets.token_bytes(4)
        writer.write(build_frame(0x1, payload, mask=mask))
        await writer.drain()

    async def mint_session() -> str:
        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        writer.write(
            b"POST /api/v1/session HTTP/1.1\r\nHost: x\r\n"
            b"Authorization: Bearer p-secret\r\nX-Radar-Room: alpha\r\n\r\n"
        )
        await writer.drain()
        status, body = await _read_http(reader)
        writer.close()
        check(b"200" in status, "session minted for expiry test")
        return json.loads(body)["session_token"]

    async def ws_recv(reader: asyncio.StreamReader) -> bytes:
        opcode, payload = await read_frame(reader)
        assert opcode == 0x1, f"expected text frame, got {opcode}"
        return payload

    # 1. Session minting requires the producer token.
    _, writer, status = await connect(
        "/api/v1/session", {"Authorization": "Bearer wrong",
                            "X-Radar-Room": "alpha"}, upgrade=False)
    check(b"401" in status, "session with wrong token -> 401")
    writer.close()

    session_token = await mint_session()

    # 1b. Minted sessions are accepted until they expire, then rejected at
    #     use (not only at the next mint).
    _, writer, status = await connect(
        f"/api/v1/stream?room=alpha&session={session_token}")
    check(b"101" in status, "fresh session accepted")
    writer.close()
    relay._sessions[session_token] = ("alpha", time.monotonic() - 1.0)
    _, writer, status = await connect(
        f"/api/v1/stream?room=alpha&session={session_token}")
    check(b"401" in status, "expired session rejected at use")
    writer.close()

    # 2. Publish path rejects a viewer token at the WS handshake.
    _, writer, status = await connect(
        "/api/v1/publish", {"Authorization": "Bearer v-secret",
                            "X-Radar-Room": "alpha"})
    check(b"401" in status, "viewer token rejected on publish")
    writer.close()

    # 3. Producer publishes three frames; a viewer connecting afterwards
    #    receives only the latest one.
    reader, writer, status = await connect(
        "/api/v1/publish", {"Authorization": "Bearer p-secret",
                            "X-Radar-Room": "alpha"})
    check(b"101" in status, "producer connected")
    for i in range(3):
        await ws_send(writer, f"frame-{i}".encode())
        await asyncio.sleep(0.02)

    vreader, vwriter, status = await connect(
        "/api/v1/stream?room=alpha&token=v-secret")
    check(b"101" in status, "viewer connected")
    got = await asyncio.wait_for(ws_recv(vreader), timeout=2)
    check(got == b"frame-2", "latest frame on connect")

    # 4. Live broadcast: the next frame reaches the connected viewer.
    await ws_send(writer, b"frame-3")
    got = await asyncio.wait_for(ws_recv(vreader), timeout=2)
    check(got == b"frame-3", "live broadcast")

    # 5. Viewer with a wrong token is rejected pre-handshake with 401.
    _, bwriter, status = await connect(
        "/api/v1/stream?room=alpha&token=nope")
    check(b"401" in status, "bad-token viewer rejected with 401")
    bwriter.close()

    # 6. Oversize frame terminates the producer connection.
    preader, pwriter, status = await connect(
        "/api/v1/publish", {"Authorization": "Bearer p-secret",
                            "X-Radar-Room": "alpha"})
    # Room exclusivity: the first producer still holds alpha, so this is 401.
    check(b"401" in status, "room producer exclusivity enforced")
    pwriter.close()

    # 7. Oversize frame on a fresh room terminates the producer connection.
    rooms_big = [RoomPolicy("big", "p-big", "v-big", viewer_rate_hz=1000)]
    relay_big = RelayServer(rooms_big)
    port_big, server_big = await relay_big.serve("127.0.0.1", 0)

    async def connect_big(path: str, headers: dict[str, str] | None = None,
                          upgrade: bool = True) -> tuple[
            asyncio.StreamReader, asyncio.StreamWriter, bytes]:
        reader, writer = await asyncio.open_connection("127.0.0.1", port_big)
        head = f"{'GET' if upgrade else 'POST'} {path} HTTP/1.1\r\nHost: x\r\n"
        if upgrade:
            key = base64.b64encode(secrets.token_bytes(16)).decode()
            head += (f"Upgrade: websocket\r\nConnection: Upgrade\r\n"
                     f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n")
        for name, value in (headers or {}).items():
            head += f"{name}: {value}\r\n"
        head += "\r\n"
        writer.write(head.encode())
        await writer.drain()
        status, _body = await _read_http(reader)
        return reader, writer, status

    preader, pwriter, status = await connect_big(
        "/api/v1/publish", {"Authorization": "Bearer p-big",
                            "X-Radar-Room": "big"})
    check(b"101" in status, "big-room producer connected")
    await ws_send(pwriter, b"x" * (512 * 1024 + 1))
    try:
        opcode, _ = await asyncio.wait_for(read_frame(preader), timeout=2)
        check(opcode == 0x8, "oversize producer closed with WS close")
    except (asyncio.IncompleteReadError, ConnectionError,
            asyncio.TimeoutError):
        check(True, "oversize producer connection terminated")

    writer.close()
    vwriter.close()
    pwriter.close()
    server.close()
    server_big.close()
    await server.wait_closed()
    await server_big.wait_closed()

    if failures != 0:
        print(f"{failures} relay loopback check(s) failed")
        return 1
    print("relay_server loopback: all checks passed")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--config", help="JSON config with a rooms list")
    args = parser.parse_args(argv)

    if args.self_test:
        return asyncio.run(_loopback_test())

    if not args.config:
        parser.print_usage()
        return 2
    with open(args.config, encoding="utf-8") as handle:
        config = json.load(handle)
    rooms = [
        RoomPolicy(
            room_id=room["room_id"],
            producer_token=room["producer_token"],
            viewer_token=room["viewer_token"],
            viewer_rate_hz=int(room.get("viewer_rate_hz", 40)),
            viewer_cap=int(room.get("viewer_cap", 16)),
        )
        for room in config.get("rooms", [])
    ]
    if not rooms:
        print("config has no rooms", file=sys.stderr)
        return 2

    async def run() -> None:
        relay = RelayServer(rooms)
        port, server = await relay.serve(args.host, args.port)
        print(f"relay listening on {args.host}:{port}")
        async with server:
            await server.serve_forever()

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
