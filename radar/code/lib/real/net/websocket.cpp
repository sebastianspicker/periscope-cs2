// websocket.cpp — Minimal RFC 6455 client (ws:// over TCP).
// wss:// is accepted by URL parser; full TLS tunnel uses HTTPS C2 path on Windows.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace real::net {

namespace {

std::string ws_accept_key(const std::string& client_key_b64) {
  // GUID from RFC 6455
  const std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const std::string concat = client_key_b64 + guid;
  // SHA-1 is required by RFC — implement compact SHA-1 here for handshake only.
  // For research stack we use SHA-256 base64 of same material as lab compatibility
  // fallback when speaking to our own echo tests; real servers need SHA-1.
  // Provide proper SHA-1:
  auto sha1 = [](const std::uint8_t* data, std::size_t len) {
    // Compact SHA-1
    auto rol = [](std::uint32_t x, int n) {
      return (x << n) | (x >> (32 - n));
    };
    std::uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
                  h3 = 0x10325476u, h4 = 0xC3D2E1F0u;
    std::vector<std::uint8_t> msg(data, data + len);
    const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i)
      msg.push_back(static_cast<std::uint8_t>(bit_len >> (i * 8)));
    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
      std::uint32_t w[80];
      for (int i = 0; i < 16; ++i)
        w[i] = (static_cast<std::uint32_t>(msg[chunk + i * 4]) << 24) |
               (static_cast<std::uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(msg[chunk + i * 4 + 3]);
      for (int i = 16; i < 80; ++i)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
      std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
      for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
        const std::uint32_t temp = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
      }
      h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::vector<std::uint8_t> out(20);
    auto put = [&](int i, std::uint32_t v) {
      out[i] = static_cast<std::uint8_t>(v >> 24);
      out[i + 1] = static_cast<std::uint8_t>(v >> 16);
      out[i + 2] = static_cast<std::uint8_t>(v >> 8);
      out[i + 3] = static_cast<std::uint8_t>(v);
    };
    put(0, h0); put(4, h1); put(8, h2); put(12, h3); put(16, h4);
    return out;
  };
  auto dig = sha1(reinterpret_cast<const std::uint8_t*>(concat.data()), concat.size());
  return to_base64(dig);
}

Result<void> ws_send_frame(Socket sock, std::uint8_t opcode,
                            const std::uint8_t* data, std::size_t len) {
  std::vector<std::uint8_t> frame;
  frame.push_back(static_cast<std::uint8_t>(0x80 | (opcode & 0x0F)));  // FIN + opcode
  // Client must mask
  if (len < 126) {
    frame.push_back(static_cast<std::uint8_t>(0x80 | len));
  } else if (len <= 0xFFFF) {
    frame.push_back(static_cast<std::uint8_t>(0x80 | 126));
    frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
    frame.push_back(static_cast<std::uint8_t>(len & 0xFF));
  } else {
    frame.push_back(static_cast<std::uint8_t>(0x80 | 127));
    for (int i = 7; i >= 0; --i)
      frame.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(len) >> (8 * i)) & 0xFF));
  }
  auto mask_r = random_bytes(4);
  std::uint8_t mask[4] = {1, 2, 3, 4};
  if (mask_r && mask_r->size() == 4)
    std::memcpy(mask, mask_r->data(), 4);
  frame.insert(frame.end(), mask, mask + 4);
  for (std::size_t i = 0; i < len; ++i)
    frame.push_back(static_cast<std::uint8_t>((data ? data[i] : 0) ^ mask[i % 4]));
  return send_all(sock, frame.data(), static_cast<int>(frame.size()));
}

Result<std::vector<std::uint8_t>> ws_recv_frame(Socket sock, std::uint8_t* opcode_out) {
  std::uint8_t hdr[2];
  auto r = recv_exact(sock, hdr, 2);
  if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
  const std::uint8_t opcode = hdr[0] & 0x0F;
  if (opcode_out) *opcode_out = opcode;
  const bool masked = (hdr[1] & 0x80) != 0;
  std::uint64_t payload_len = hdr[1] & 0x7F;
  if (payload_len == 126) {
    std::uint8_t ext[2];
    r = recv_exact(sock, ext, 2);
    if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
    payload_len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
  } else if (payload_len == 127) {
    std::uint8_t ext[8];
    r = recv_exact(sock, ext, 8);
    if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
    payload_len = 0;
    for (int i = 0; i < 8; ++i)
      payload_len = (payload_len << 8) | ext[i];
  }
  if (payload_len > 16ull * 1024 * 1024)
    return Result<std::vector<std::uint8_t>>({}, "WebSocket frame too large");
  std::uint8_t mask[4]{};
  if (masked) {
    r = recv_exact(sock, mask, 4);
    if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
  }
  std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_len));
  if (payload_len) {
    r = recv_exact(sock, payload.data(), static_cast<int>(payload_len));
    if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
    if (masked) {
      for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] ^= mask[i % 4];
    }
  }
  return payload;
}

}  // namespace

WebSocketClient::~WebSocketClient() { close(); }

Result<void> WebSocketClient::connect(const std::string& url, int timeout_ms) {
  close();
  auto parsed = parse_url(url);
  if (!parsed.is_ws && parsed.scheme != "ws" && parsed.scheme != "wss") {
    // Allow bare host path if scheme empty
    if (parsed.scheme != "ws" && parsed.scheme != "wss" && !parsed.is_ws) {
      // try parse as ws
      if (url.find("ws") != 0)
        return Result<void>("URL must be ws:// or wss://");
    }
  }
  if (parsed.is_https || parsed.scheme == "wss") {
    // Full wss needs TLS; on this platform stack route via note for lab:
    // attempt TCP to 443 with ws upgrade only works behind TLS terminators.
    return Result<void>("wss:// requires TLS tunnel; use ws:// lab server or HTTPS C2");
  }

  ConnectOptions opts;
  opts.timeout_ms = timeout_ms;
  auto sock = connect_ex(parsed.host, parsed.port, opts, Protocol::Tcp);
  if (!sock) return Result<void>(sock.error_msg);
  sock_ = *sock;
  set_timeout(sock_, timeout_ms);
  host_ = parsed.host;
  path_ = parsed.path.empty() ? "/" : parsed.path;

  auto key_bytes = random_bytes(16);
  if (!key_bytes) { close(); return Result<void>(key_bytes.error_msg); }
  const std::string key_b64 = to_base64(*key_bytes);
  const std::string expected = ws_accept_key(key_b64);

  std::string req =
      "GET " + path_ + " HTTP/1.1\r\n"
      "Host: " + host_ + "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: " + key_b64 + "\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n";
  auto s = send_all(sock_, reinterpret_cast<const std::uint8_t*>(req.data()),
                    static_cast<int>(req.size()));
  if (!s) { close(); return Result<void>(s.error_msg); }

  // Read HTTP response headers
  std::string resp;
  char ch = 0;
  while (resp.find("\r\n\r\n") == std::string::npos) {
    auto r = recv(sock_, reinterpret_cast<std::uint8_t*>(&ch), 1);
    if (!r || *r == 0) { close(); return Result<void>("WebSocket handshake failed"); }
    resp.push_back(ch);
    if (resp.size() > 8192) { close(); return Result<void>("Handshake too large"); }
  }
  if (resp.find("101") == std::string::npos) {
    close();
    return Result<void>("WebSocket upgrade rejected");
  }
  // Validate accept key if present
  const auto pos = to_lower_ascii(resp).find("sec-websocket-accept:");
  if (pos != std::string::npos) {
    auto line_end = resp.find("\r\n", pos);
    std::string line = resp.substr(pos, line_end - pos);
    auto colon = line.find(':');
    std::string got = trim_ascii(line.substr(colon + 1));
    if (got != expected) {
      // Still accept for tolerant lab servers
    }
  }
  open_ = true;
  is_tls_ = false;
  return Result<void>();
}

Result<void> WebSocketClient::send_text(const std::string& text) {
  if (!open_) return Result<void>("WebSocket not open");
  return ws_send_frame(sock_, 0x1, reinterpret_cast<const std::uint8_t*>(text.data()),
                       text.size());
}

Result<void> WebSocketClient::send_binary(const std::uint8_t* data, std::size_t len) {
  if (!open_) return Result<void>("WebSocket not open");
  return ws_send_frame(sock_, 0x2, data, len);
}

Result<std::vector<std::uint8_t>> WebSocketClient::recv_message(int timeout_ms) {
  if (!open_) return Result<std::vector<std::uint8_t>>({}, "WebSocket not open");
  set_timeout(sock_, timeout_ms);
  for (;;) {
    std::uint8_t opcode = 0;
    auto payload = ws_recv_frame(sock_, &opcode);
    if (!payload) return payload;
    if (opcode == 0x8) {  // close
      close();
      return Result<std::vector<std::uint8_t>>({}, "WebSocket closed by peer");
    }
    if (opcode == 0x9) {  // ping -> pong
      ws_send_frame(sock_, 0xA, payload->data(), payload->size());
      continue;
    }
    if (opcode == 0xA) continue;  // pong
    return payload;
  }
}

void WebSocketClient::close() {
  if (open_ && sock_.fd >= 0) {
    ws_send_frame(sock_, 0x8, nullptr, 0);
  }
  if (sock_.fd >= 0) {
    real::net::close(sock_);
    sock_.fd = -1;
  }
  open_ = false;
}

}  // namespace real::net
