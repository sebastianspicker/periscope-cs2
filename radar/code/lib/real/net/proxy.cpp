// proxy.cpp — SOCKS5 and HTTP CONNECT proxy tunnels.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace real::net {

Result<Socket> socks5_connect(const std::string& proxy_host, std::uint16_t proxy_port,
                               const std::string& target_host, std::uint16_t target_port,
                               const std::string& user, const std::string& pass,
                               int timeout_ms) {
  ConnectOptions opts;
  opts.timeout_ms = timeout_ms;
  opts.proxy_type = ConnectOptions::ProxyType::None;
  auto sock = connect_ex(proxy_host, proxy_port, opts, Protocol::Tcp);
  if (!sock) return Result<Socket>(Socket{-1}, sock.error_msg);
  set_timeout(*sock, timeout_ms);

  // Greeting
  std::uint8_t greet[4];
  greet[0] = 0x05;  // VER
  if (!user.empty()) {
    greet[1] = 0x02;  // NMETHODS
    greet[2] = 0x00;  // NO AUTH
    greet[3] = 0x02;  // USERNAME/PASSWORD
    auto s = send_all(*sock, greet, 4);
    if (!s) { close(*sock); return Result<Socket>(Socket{-1}, s.error_msg); }
  } else {
    greet[1] = 0x01;
    greet[2] = 0x00;
    auto s = send_all(*sock, greet, 3);
    if (!s) { close(*sock); return Result<Socket>(Socket{-1}, s.error_msg); }
  }

  std::uint8_t method_resp[2];
  auto r = recv_exact(*sock, method_resp, 2);
  if (!r) { close(*sock); return Result<Socket>(Socket{-1}, r.error_msg); }
  if (method_resp[0] != 0x05) {
    close(*sock);
    return Result<Socket>(Socket{-1}, "Not a SOCKS5 proxy");
  }

  if (method_resp[1] == 0x02) {
    // Username/password auth (RFC 1929)
    if (user.size() > 255 || pass.size() > 255) {
      close(*sock);
      return Result<Socket>(Socket{-1}, "SOCKS5 credentials too long");
    }
    std::vector<std::uint8_t> auth;
    auth.push_back(0x01);
    auth.push_back(static_cast<std::uint8_t>(user.size()));
    auth.insert(auth.end(), user.begin(), user.end());
    auth.push_back(static_cast<std::uint8_t>(pass.size()));
    auth.insert(auth.end(), pass.begin(), pass.end());
    auto s = send_all(*sock, auth.data(), static_cast<int>(auth.size()));
    if (!s) { close(*sock); return Result<Socket>(Socket{-1}, s.error_msg); }
    std::uint8_t auth_resp[2];
    r = recv_exact(*sock, auth_resp, 2);
    if (!r || auth_resp[1] != 0x00) {
      close(*sock);
      return Result<Socket>(Socket{-1}, "SOCKS5 auth failed");
    }
  } else if (method_resp[1] != 0x00) {
    close(*sock);
    return Result<Socket>(Socket{-1}, "SOCKS5 method rejected");
  }

  // CONNECT request with domain name
  if (target_host.size() > 255) {
    close(*sock);
    return Result<Socket>(Socket{-1}, "Target host too long for SOCKS5");
  }
  std::vector<std::uint8_t> req;
  req.push_back(0x05);  // VER
  req.push_back(0x01);  // CONNECT
  req.push_back(0x00);  // RSV
  req.push_back(0x03);  // ATYP = DOMAIN
  req.push_back(static_cast<std::uint8_t>(target_host.size()));
  req.insert(req.end(), target_host.begin(), target_host.end());
  req.push_back(static_cast<std::uint8_t>((target_port >> 8) & 0xFF));
  req.push_back(static_cast<std::uint8_t>(target_port & 0xFF));

  auto s = send_all(*sock, req.data(), static_cast<int>(req.size()));
  if (!s) { close(*sock); return Result<Socket>(Socket{-1}, s.error_msg); }

  std::uint8_t hdr[4];
  r = recv_exact(*sock, hdr, 4);
  if (!r) { close(*sock); return Result<Socket>(Socket{-1}, r.error_msg); }
  if (hdr[0] != 0x05 || hdr[1] != 0x00) {
    close(*sock);
    return Result<Socket>(Socket{-1},
        "SOCKS5 CONNECT failed status=" + std::to_string(hdr[1]));
  }

  // Consume bind address
  std::size_t addr_len = 0;
  if (hdr[3] == 0x01) addr_len = 4;
  else if (hdr[3] == 0x04) addr_len = 16;
  else if (hdr[3] == 0x03) {
    std::uint8_t dlen = 0;
    r = recv_exact(*sock, &dlen, 1);
    if (!r) { close(*sock); return Result<Socket>(Socket{-1}, r.error_msg); }
    addr_len = dlen;
  } else {
    close(*sock);
    return Result<Socket>(Socket{-1}, "SOCKS5 unknown ATYP");
  }
  std::vector<std::uint8_t> skip(addr_len + 2);
  r = recv_exact(*sock, skip.data(), static_cast<int>(skip.size()));
  if (!r) { close(*sock); return Result<Socket>(Socket{-1}, r.error_msg); }

  set_nodelay(*sock, true);
  return *sock;
}

Result<Socket> http_connect_proxy(const std::string& proxy_host, std::uint16_t proxy_port,
                                   const std::string& target_host, std::uint16_t target_port,
                                   int timeout_ms) {
  ConnectOptions opts;
  opts.timeout_ms = timeout_ms;
  auto sock = connect_ex(proxy_host, proxy_port, opts, Protocol::Tcp);
  if (!sock) return Result<Socket>(Socket{-1}, sock.error_msg);
  set_timeout(*sock, timeout_ms);

  const std::string req =
      "CONNECT " + target_host + ":" + std::to_string(target_port) + " HTTP/1.1\r\n"
      "Host: " + target_host + ":" + std::to_string(target_port) + "\r\n"
      "Proxy-Connection: Keep-Alive\r\n"
      "\r\n";
  auto s = send_all(*sock, reinterpret_cast<const std::uint8_t*>(req.data()),
                    static_cast<int>(req.size()));
  if (!s) { close(*sock); return Result<Socket>(Socket{-1}, s.error_msg); }

  // Read until end of headers
  std::string resp;
  char ch = 0;
  while (resp.find("\r\n\r\n") == std::string::npos) {
    auto r = recv(*sock, reinterpret_cast<std::uint8_t*>(&ch), 1);
    if (!r || *r == 0) {
      close(*sock);
      return Result<Socket>(Socket{-1}, "HTTP CONNECT: no response");
    }
    resp.push_back(ch);
    if (resp.size() > 8192) {
      close(*sock);
      return Result<Socket>(Socket{-1}, "HTTP CONNECT: response too large");
    }
  }

  // Status line: HTTP/1.x 200
  const auto sp = resp.find(' ');
  if (sp == std::string::npos) {
    close(*sock);
    return Result<Socket>(Socket{-1}, "HTTP CONNECT: bad status line");
  }
  int code = 0;
  try {
    code = std::stoi(resp.substr(sp + 1, 3));
  } catch (...) {
    close(*sock);
    return Result<Socket>(Socket{-1}, "HTTP CONNECT: bad status code");
  }
  if (code < 200 || code >= 300) {
    close(*sock);
    return Result<Socket>(Socket{-1},
        "HTTP CONNECT failed status=" + std::to_string(code));
  }
  set_nodelay(*sock, true);
  return *sock;
}

}  // namespace real::net
