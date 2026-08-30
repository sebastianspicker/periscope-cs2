// net_internal.hpp — Internal helpers shared across net split files.
#pragma once

#include "real/net/net_client.hpp"
#include "real/net/net_crypto.hpp"

#include <string>
#include <vector>
#include <map>

namespace real::net {

// ── Platform socket init ───────────────────────────────────────────
#if LR_PLATFORM_WINDOWS
Result<void> ensure_winsock();
#endif
int close_socket(int fd);

// parse_url is public in net_client.hpp

/// Case-insensitive header lookup.
std::string header_get(const std::map<std::string, std::string>& headers,
                       const std::string& key);

/// Parse HTTP/1.x status line + headers + body from raw bytes.
/// Handles Content-Length and chunked transfer encoding.
Result<HttpResponse> parse_http_response(const std::vector<std::uint8_t>& raw);

/// Decode chunked transfer body.
Result<std::vector<std::uint8_t>> decode_chunked(const std::uint8_t* data, std::size_t len);

/// Lowercase ASCII copy.
std::string to_lower_ascii(std::string s);

/// Trim whitespace.
std::string trim_ascii(std::string s);

/// Join host:port for display.
std::string host_port(const std::string& host, std::uint16_t port);

/// Perform raw HTTP/1.1 request over an already-connected socket.
Result<HttpResponse> http_exchange_raw(
    Socket sock, const std::string& method, const std::string& path,
    const std::string& host, const std::map<std::string, std::string>& headers,
    const std::uint8_t* body, std::size_t body_len, int timeout_ms);

/// WinHTTP-backed request (HTTPS + HTTP). Used by https_*_ex on Windows.
Result<HttpResponse> winhttp_request(
    const std::string& method, const std::string& url,
    const HttpRequestOptions& opts,
    const std::uint8_t* body = nullptr, std::size_t body_len = 0,
    const std::string& content_type = {});

}  // namespace real::net
