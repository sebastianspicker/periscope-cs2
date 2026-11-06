# Radar public relay (ledger R6)

An outbound-only relay so a game PC with no inbound ports can share a radar
session with remote viewers. First-party Python (stdlib only): no third-party
server framework, no bundled binaries.

## Topology and trust

```
game PC --(outbound WSS, producer token)--> Caddy :443 --> relay_server.py
browser ------------------------------------------> Caddy :443 --> relay_server.py
```

- The relay never connects to the gaming machine; the producer dials out.
- Producer and viewer credentials are separate secrets; a viewer token cannot
  publish (enforced pre-handshake).
- Per room, **only the latest frame is retained** - no history, no
  persistence.
- HTTP/WS tokens are not transport security; deploy behind Caddy TLS and
  never expose the relay directly.

## Running

```bash
cat > relay.json <<'JSON'
{"rooms": [
  {"room_id": "alpha",
   "producer_token": "<long random producer secret>",
   "viewer_token": "<long random viewer secret>",
   "viewer_rate_hz": 40, "viewer_cap": 16}
]}
JSON
python3 radar/relay/relay_server.py --config relay.json --port 8787
```

Endpoints:

| Endpoint | Role | Auth |
|---|---|---|
| `POST /api/v1/session` | mint a short-lived browser session | producer bearer token + `X-Radar-Room` |
| `WS  /api/v1/publish` | producer pushes the latest snapshot | producer bearer token + `X-Radar-Room` |
| `WS  /api/v1/stream` | viewer receives latest-on-connect, then live pushes | `?room=..&token=..` or `?room=..&session=..` |

Limits: 512 KiB frame cap (violating producers are disconnected), per-room
producer exclusivity, per-room viewer cap, token-bucket frame rate limiting
with bounded-latest broadcasting (a rate-limited viewer skips a frame and
catches up on the next - it never queues).

## Verification

```bash
python3 radar/relay/relay_core.py --self-test      # policy: rooms, tokens, caps, backoff
python3 radar/relay/relay_server.py --self-test    # loopback integration over real sockets
```

Both run in Radar CI on every PR (see `.github/workflows/radar-ci.yml`).

## Frame format

The relay is frame-agnostic: it forwards opaque payload bytes with a
monotonic per-room sequence number. The expected payload is the Radar track's
protocol v1 snapshot (`radar/src/domain/radar/snapshot.hpp`).
