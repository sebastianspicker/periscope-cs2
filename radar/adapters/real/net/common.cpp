// common.cpp — Shared helpers: Winsock lifetime, URL parsing, HTTP body parse.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace real::net {

// ── Winsock lifetime ───────────────────────────────────────────────
#if LR_PLATFORM_WINDOWS
static bool winsock_initialized_ = false;

Result<void> ensure_winsock() {
  if (!winsock_initialized_) {
    WSADATA wsa{};
    const int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) return Result<void>("WSAStartup failed");
    winsock_initialized_ = true;
  }
  return Result<void>();
}
#endif

int close_socket(int fd) {
  if (fd < 0) return 0;
#if LR_PLATFORM_WINDOWS
  return closesocket(fd);
#else
  return ::close(fd);
#endif
}

// ── String helpers ─────────────────────────────────────────────────

std::string to_lower_ascii(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string trim_ascii(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' ||
                        s.front() == '\r' || s.front() == '\n'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
                        s.back() == '\r' || s.back() == '\n'))
    s.pop_back();
  return s;
}

std::string host_port(const std::string& host, std::uint16_t port) {
  return host + ":" + std::to_string(port);
}

std::string header_get(const std::map<std::string, std::string>& headers,
                       const std::string& key) {
  const std::string lk = to_lower_ascii(key);
  for (const auto& kv : headers) {
    if (to_lower_ascii(kv.first) == lk) return kv.second;
  }
  return {};
}

// ── URL parsing ────────────────────────────────────────────────────

ParsedUrl parse_url(const std::string& url) {
  ParsedUrl result;
  result.path = "/";
  std::string remaining = url;

  if (remaining.size() >= 8 && to_lower_ascii(remaining.substr(0, 8)) == "https://") {
    result.scheme = "https";
    result.is_https = true;
    result.port = 443;
    remaining = remaining.substr(8);
  } else if (remaining.size() >= 7 && to_lower_ascii(remaining.substr(0, 7)) == "http://") {
    result.scheme = "http";
    result.is_https = false;
    result.port = 80;
    remaining = remaining.substr(7);
  } else if (remaining.size() >= 6 && to_lower_ascii(remaining.substr(0, 6)) == "wss://") {
    result.scheme = "wss";
    result.is_https = true;
    result.is_ws = true;
    result.port = 443;
    remaining = remaining.substr(6);
  } else if (remaining.size() >= 5 && to_lower_ascii(remaining.substr(0, 5)) == "ws://") {
    result.scheme = "ws";
    result.is_https = false;
    result.is_ws = true;
    result.port = 80;
    remaining = remaining.substr(5);
  } else {
    // host:port[/path] bare form
    result.scheme = "tcp";
    result.port = 80;
  }

  // Strip userinfo
  const auto at = remaining.find('@');
  const auto slash_probe = remaining.find('/');
  if (at != std::string::npos && (slash_probe == std::string::npos || at < slash_probe))
    remaining = remaining.substr(at + 1);

  const auto slash_pos = remaining.find('/');
  const auto qpos = remaining.find('?');
  const auto path_start = slash_pos;

  std::string authority = (slash_pos == std::string::npos)
      ? remaining
      : remaining.substr(0, slash_pos);

  // IPv6 [addr]:port
  if (!authority.empty() && authority.front() == '[') {
    const auto rb = authority.find(']');
    if (rb != std::string::npos) {
      result.host = authority.substr(1, rb - 1);
      if (rb + 1 < authority.size() && authority[rb + 1] == ':') {
        try {
          result.port = static_cast<std::uint16_t>(
              std::stoi(authority.substr(rb + 2)));
        } catch (...) {
        }
      }
    } else {
      result.host = authority;
    }
  } else {
    const auto colon_pos = authority.rfind(':');
    if (colon_pos != std::string::npos) {
      result.host = authority.substr(0, colon_pos);
      try {
        result.port = static_cast<std::uint16_t>(
            std::stoi(authority.substr(colon_pos + 1)));
      } catch (...) {
      }
    } else {
      result.host = authority;
    }
  }

  if (path_start != std::string::npos) {
    result.path = remaining.substr(path_start);
  } else if (qpos != std::string::npos && slash_pos == std::string::npos) {
    result.path = "/" + remaining.substr(qpos);  // unlikely bare ?query
  }

  return result;
}

// ── Chunked decode ─────────────────────────────────────────────────

Result<std::vector<std::uint8_t>> decode_chunked(const std::uint8_t* data,
                                                  std::size_t len) {
  std::vector<std::uint8_t> out;
  std::size_t i = 0;
  while (i < len) {
    // Read hex size line
    std::size_t line_end = i;
    while (line_end + 1 < len &&
           !(data[line_end] == '\r' && data[line_end + 1] == '\n'))
      ++line_end;
    if (line_end + 1 >= len)
      return Result<std::vector<std::uint8_t>>({}, "Truncated chunk size");
    std::string hex_size(reinterpret_cast<const char*>(data + i), line_end - i);
    // Strip chunk extensions
    const auto sc = hex_size.find(';');
    if (sc != std::string::npos) hex_size = hex_size.substr(0, sc);
    hex_size = trim_ascii(hex_size);
    std::size_t chunk_len = 0;
    try {
      chunk_len = static_cast<std::size_t>(std::stoul(hex_size, nullptr, 16));
    } catch (...) {
      return Result<std::vector<std::uint8_t>>({}, "Bad chunk size");
    }
    i = line_end + 2;
    if (chunk_len == 0) break;
    if (i + chunk_len > len)
      return Result<std::vector<std::uint8_t>>({}, "Truncated chunk data");
    out.insert(out.end(), data + i, data + i + chunk_len);
    i += chunk_len;
    if (i + 1 < len && data[i] == '\r' && data[i + 1] == '\n')
      i += 2;
  }
  return out;
}

// ── HTTP response parse ────────────────────────────────────────────

Result<HttpResponse> parse_http_response(const std::vector<std::uint8_t>& raw) {
  HttpResponse result;
  if (raw.empty())
    return Result<HttpResponse>({}, "Empty HTTP response");

  const std::string rstr(reinterpret_cast<const char*>(raw.data()), raw.size());
  const auto header_end = rstr.find("\r\n\r\n");
  if (header_end == std::string::npos)
    return Result<HttpResponse>({}, "No HTTP header terminator");

  const std::string header_block = rstr.substr(0, header_end);
  std::size_t line_start = 0;
  bool first = true;
  while (line_start < header_block.size()) {
    auto line_end = header_block.find("\r\n", line_start);
    if (line_end == std::string::npos) line_end = header_block.size();
    std::string line = header_block.substr(line_start, line_end - line_start);
    line_start = line_end + 2;
    if (first) {
      first = false;
      // HTTP/1.x CODE reason
      const auto s1 = line.find(' ');
      if (s1 != std::string::npos) {
        const auto s2 = line.find(' ', s1 + 1);
        try {
          result.status_code = std::stoi(
              line.substr(s1 + 1, (s2 == std::string::npos ? line.size() : s2) - s1 - 1));
        } catch (...) {
          result.status_code = 0;
        }
      }
      continue;
    }
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = trim_ascii(line.substr(0, colon));
    std::string val = trim_ascii(line.substr(colon + 1));
    result.headers[key] = val;
    if (to_lower_ascii(key) == "content-type")
      result.content_type = val;
  }

  const std::size_t body_off = header_end + 4;
  const std::uint8_t* body_ptr = raw.data() + body_off;
  const std::size_t body_len = raw.size() - body_off;

  const std::string te = to_lower_ascii(header_get(result.headers, "Transfer-Encoding"));
  if (te.find("chunked") != std::string::npos) {
    auto decoded = decode_chunked(body_ptr, body_len);
    if (!decoded) return Result<HttpResponse>({}, decoded.error_msg);
    result.body = std::move(*decoded);
  } else {
    const std::string cl = header_get(result.headers, "Content-Length");
    if (!cl.empty()) {
      try {
        const std::size_t n = static_cast<std::size_t>(std::stoul(cl));
        result.body.assign(body_ptr, body_ptr + (std::min)(n, body_len));
      } catch (...) {
        result.body.assign(body_ptr, body_ptr + body_len);
      }
    } else {
      result.body.assign(body_ptr, body_ptr + body_len);
    }
  }
  return result;
}

// ── Raw HTTP exchange over socket ──────────────────────────────────

Result<HttpResponse> http_exchange_raw(
    Socket sock, const std::string& method, const std::string& path,
    const std::string& host, const std::map<std::string, std::string>& headers,
    const std::uint8_t* body, std::size_t body_len, int timeout_ms) {
  set_timeout(sock, timeout_ms);

  std::string req = method + " " + path + " HTTP/1.1\r\n";
  req += "Host: " + host + "\r\n";
  bool has_ua = false;
  bool has_cl = false;
  bool has_conn = false;
  for (const auto& kv : headers) {
    const auto lk = to_lower_ascii(kv.first);
    if (lk == "host") continue;
    if (lk == "user-agent") has_ua = true;
    if (lk == "content-length") has_cl = true;
    if (lk == "connection") has_conn = true;
    req += kv.first + ": " + kv.second + "\r\n";
  }
  if (!has_ua) req += "User-Agent: EducationalLab/2.0\r\n";
  if (!has_cl && body_len > 0)
    req += "Content-Length: " + std::to_string(body_len) + "\r\n";
  if (!has_conn) req += "Connection: close\r\n";
  req += "\r\n";

  auto sent = send_all(sock, reinterpret_cast<const std::uint8_t*>(req.data()),
                       static_cast<int>(req.size()));
  if (!sent) return Result<HttpResponse>({}, sent.error_msg);
  if (body && body_len > 0) {
    sent = send_all(sock, body, static_cast<int>(body_len));
    if (!sent) return Result<HttpResponse>({}, sent.error_msg);
  }

  std::vector<std::uint8_t> raw;
  std::uint8_t buf[4096];
  for (;;) {
    auto received = recv(sock, buf, static_cast<int>(sizeof(buf)));
    if (!received) {
      if (!raw.empty()) break;  // timeout with data — try parse
      return Result<HttpResponse>({}, received.error_msg);
    }
    if (*received == 0) break;
    raw.insert(raw.end(), buf, buf + *received);
  }
  return parse_http_response(raw);
}

}  // namespace real::net
