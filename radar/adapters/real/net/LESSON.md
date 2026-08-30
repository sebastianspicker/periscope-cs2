# Networking research adapter

This opt-in adapter contains lab implementations for C2, offset CDN/SaaS, DNS
tunneling, proxies, WebSocket, and multi-process named-pipe IPC. CMake uses an
explicit source manifest to build `ac_real_net` when `LR_ENABLE_REAL_NET=ON`.

## Domain files

| File | Domain | Functions / types |
|------|--------|-------------------|
| `common.cpp` | Shared | `ensure_winsock`, `close_socket`, `parse_url`, HTTP parse, chunked decode |
| `crypto.cpp` | Crypto | SHA-256, HMAC-SHA256, ChaCha20, HKDF, CRC32, Base64/32/Hex, SecureBuffer, CSPRNG |
| `socket.cpp` | Sockets | `connect`/`connect_ex`, `listen_tcp`, `accept`, `send_all`/`recv_exact`, UDP, timeouts, Nagle, keepalive |
| `http.cpp` | HTTP(S) | `https_get`/`https_post` (+ `_ex`), WinHTTP TLS 1.2+, redirects, cert pin, domain front Host |
| `proxy.cpp` | Proxies | SOCKS5 (user/pass), HTTP CONNECT |
| `c2_client.cpp` | C2 | ACL2 framed protocol, ChaCha20+HMAC, TCP + HTTPS POST, auth, beacon scheduler, failover |
| `dns.cpp` | DNS | base32 tunnel encode/decode, wire query/response, UDP resolve, DoH |
| `pipe.cpp` | IPC | Named pipes (server/client, timeouts), length-prefixed frames |
| `websocket.cpp` | WS | RFC 6455 client (`ws://`), masked frames, ping/pong |
| `offset_fetch.cpp` | Offsets | `fetch_offsets` + CRC verify, schema SaaS, encrypted blob, `build_offset_blob` |

Headers: `net_client.hpp` (public), `net_crypto.hpp`, `net_internal.hpp`, `offset_fetch.hpp`.

## C2 wire format (ACL2)

```
magic u32 LE ('ACL2') | type u8 | flags u8 | seq u32 | len u32 | payload | mac[32]?
```

Flags: `Encrypted` (ChaCha20 seal), `HasMac` (HMAC-SHA256 over header+payload).

Link: Windows needs `ws2_32 winhttp bcrypt crypt32` (wired in CMake when `LR_ENABLE_REAL_NET`).
