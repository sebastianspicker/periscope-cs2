// http.cpp — HTTP/HTTPS client.
// Windows: WinHTTP with TLS 1.2+, redirects, custom headers, optional proxy.
// Non-Windows: raw HTTP/1.1 over TCP (HTTPS requires WinHTTP or external TLS).

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <wincrypt.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#  pragma comment(lib, "crypt32.lib")
#endif

namespace real::net {

namespace {

HttpRequestOptions make_opts(const std::string& user_agent, int timeout_ms) {
  HttpRequestOptions o;
  if (!user_agent.empty()) o.user_agent = user_agent;
  o.timeout_ms = timeout_ms;
  return o;
}

}  // namespace

#if LR_PLATFORM_WINDOWS

namespace {

std::wstring to_wide(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                    nullptr, 0);
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
  return w;
}

std::string to_utf8(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                    nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n,
                      nullptr, nullptr);
  return s;
}

}  // namespace

Result<HttpResponse> winhttp_request(
    const std::string& method, const std::string& url,
    const HttpRequestOptions& opts,
    const std::uint8_t* body, std::size_t body_len,
    const std::string& content_type) {
  auto parsed = parse_url(url);
  if (parsed.host.empty())
    return Result<HttpResponse>({}, "Invalid URL");

  const bool secure = parsed.is_https;
  INTERNET_PORT port = parsed.port;

  HINTERNET session = WinHttpOpen(
      to_wide(opts.user_agent).c_str(),
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);
  if (!session) return os_error("WinHttpOpen");

  // Timeouts: resolve, connect, send, receive
  WinHttpSetTimeouts(session, opts.timeout_ms, opts.timeout_ms, opts.timeout_ms,
                     opts.timeout_ms);

  if (!opts.proxy.empty()) {
    auto p = parse_url(opts.proxy.find("://") == std::string::npos
                           ? ("http://" + opts.proxy)
                           : opts.proxy);
    WINHTTP_PROXY_INFO pi{};
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    std::wstring proxy_w = to_wide(p.host + ":" + std::to_string(p.port));
    pi.lpszProxy = const_cast<LPWSTR>(proxy_w.c_str());
    pi.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
    WinHttpSetOption(session, WINHTTP_OPTION_PROXY, &pi, sizeof(pi));
  }

  HINTERNET conn = WinHttpConnect(session, to_wide(parsed.host).c_str(), port, 0);
  if (!conn) {
    WinHttpCloseHandle(session);
    return os_error("WinHttpConnect");
  }

  DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(
      conn, to_wide(method).c_str(), to_wide(parsed.path).c_str(),
      nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!req) {
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return os_error("WinHttpOpenRequest");
  }

  if (secure) {
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(req, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    if (opts.insecure_skip_verify) {
      DWORD sec_flags =
          SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
          SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
      WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }
  }

  // Redirect policy
  DWORD redirect_policy = opts.follow_redirects
      ? WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP
      : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
                   sizeof(redirect_policy));

  std::wstring headers_w;
  if (!opts.front_host.empty())
    headers_w += L"Host: " + to_wide(opts.front_host) + L"\r\n";
  if (!content_type.empty())
    headers_w += L"Content-Type: " + to_wide(content_type) + L"\r\n";
  for (const auto& kv : opts.headers) {
    if (to_lower_ascii(kv.first) == "host" && !opts.front_host.empty())
      continue;
    headers_w += to_wide(kv.first) + L": " + to_wide(kv.second) + L"\r\n";
  }
  if (!headers_w.empty()) {
    WinHttpAddRequestHeaders(req, headers_w.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  const BOOL ok = WinHttpSendRequest(
      req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
      body_len ? const_cast<LPVOID>(static_cast<const void*>(body)) : WINHTTP_NO_REQUEST_DATA,
      static_cast<DWORD>(body_len), static_cast<DWORD>(body_len), 0);
  if (!ok) {
    const auto err = os_error("WinHttpSendRequest");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return Result<HttpResponse>({}, err.error_msg);
  }

  if (!WinHttpReceiveResponse(req, nullptr)) {
    const auto err = os_error("WinHttpReceiveResponse");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return Result<HttpResponse>({}, err.error_msg);
  }

  // Optional cert pin (thumbprint of server cert)
  if (!opts.cert_pin_sha256_hex.empty() && secure) {
    PCCERT_CONTEXT cert = nullptr;
    DWORD cert_size = sizeof(cert);
    if (WinHttpQueryOption(req, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cert, &cert_size) &&
        cert) {
      auto digest = sha256(cert->pbCertEncoded, cert->cbCertEncoded);
      const std::string got = to_hex(digest);
      CertFreeCertificateContext(cert);
      if (to_lower_ascii(got) != to_lower_ascii(opts.cert_pin_sha256_hex)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return Result<HttpResponse>({}, "Certificate pin mismatch");
      }
    }
  }

  HttpResponse response;
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(req,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                      WINHTTP_NO_HEADER_INDEX);
  response.status_code = static_cast<int>(status);

  // Content-Type
  wchar_t ct_buf[256]{};
  DWORD ct_size = sizeof(ct_buf);
  if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                          ct_buf, &ct_size, WINHTTP_NO_HEADER_INDEX)) {
    response.content_type = to_utf8(ct_buf);
    response.headers["Content-Type"] = response.content_type;
  }

  // Raw headers
  DWORD hdr_size = 0;
  WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                      WINHTTP_NO_OUTPUT_BUFFER, &hdr_size, WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && hdr_size > 0) {
    std::wstring raw_hdr(hdr_size / sizeof(wchar_t), L'\0');
    if (WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                            WINHTTP_HEADER_NAME_BY_INDEX, &raw_hdr[0], &hdr_size,
                            WINHTTP_NO_HEADER_INDEX)) {
      const std::string hdrs = to_utf8(raw_hdr);
      std::size_t pos = 0;
      bool first = true;
      while (pos < hdrs.size()) {
        auto end = hdrs.find("\r\n", pos);
        if (end == std::string::npos) break;
        std::string line = hdrs.substr(pos, end - pos);
        pos = end + 2;
        if (first) { first = false; continue; }
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        response.headers[trim_ascii(line.substr(0, colon))] =
            trim_ascii(line.substr(colon + 1));
      }
    }
  }

  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(req, &avail)) break;
    if (avail == 0) break;
    std::vector<std::uint8_t> chunk(avail);
    DWORD read = 0;
    if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0) break;
    response.body.insert(response.body.end(), chunk.begin(),
                         chunk.begin() + static_cast<std::ptrdiff_t>(read));
  }

  response.final_url = url;
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(session);
  return response;
}

#else  // !Windows — raw HTTP only

Result<HttpResponse> winhttp_request(
    const std::string& method, const std::string& url,
    const HttpRequestOptions& opts,
    const std::uint8_t* body, std::size_t body_len,
    const std::string& content_type) {
  auto parsed = parse_url(url);
  if (parsed.host.empty())
    return Result<HttpResponse>({}, "Invalid URL");
  if (parsed.is_https)
    return Result<HttpResponse>({}, "HTTPS requires WinHTTP (Windows) or external TLS");

  ConnectOptions copts;
  copts.timeout_ms = opts.timeout_ms;
  auto sock = connect_ex(parsed.host, parsed.port, copts, Protocol::Tcp);
  if (!sock) return Result<HttpResponse>({}, sock.error_msg);

  std::map<std::string, std::string> headers = opts.headers;
  if (!opts.user_agent.empty()) headers["User-Agent"] = opts.user_agent;
  if (!content_type.empty()) headers["Content-Type"] = content_type;
  if (!opts.front_host.empty()) headers["Host"] = opts.front_host;

  auto resp = http_exchange_raw(*sock, method, parsed.path,
                                opts.front_host.empty() ? parsed.host : opts.front_host,
                                headers, body, body_len, opts.timeout_ms);
  close(*sock);
  if (resp) resp->final_url = url;
  return resp;
}

#endif

Result<HttpResponse> https_get_ex(const std::string& url,
                                   const HttpRequestOptions& opts) {
  auto resp = winhttp_request("GET", url, opts, nullptr, 0, {});
  if (!resp) return resp;

  // Manual redirect follow for raw path / when needed
  int redirects = 0;
  while (opts.follow_redirects && redirects < opts.max_redirects &&
         (resp->status_code == 301 || resp->status_code == 302 ||
          resp->status_code == 303 || resp->status_code == 307 ||
          resp->status_code == 308)) {
    const std::string loc = header_get(resp->headers, "Location");
    if (loc.empty()) break;
    std::string next = loc;
    if (loc.find("://") == std::string::npos) {
      auto base = parse_url(url);
      next = (base.is_https ? "https://" : "http://") + base.host;
      if (loc.empty() || loc[0] != '/') next += "/";
      next += loc;
    }
    resp = winhttp_request("GET", next, opts, nullptr, 0, {});
    if (!resp) return resp;
    resp->final_url = next;
    ++redirects;
  }
  return resp;
}

Result<HttpResponse> https_post_ex(const std::string& url,
                                    const std::vector<std::uint8_t>& body,
                                    const HttpRequestOptions& opts,
                                    const std::string& content_type) {
  const std::string ct =
      content_type.empty() ? "application/octet-stream" : content_type;
  return winhttp_request("POST", url, opts, body.data(), body.size(), ct);
}

Result<HttpResponse> https_get(const std::string& url,
                                const std::string& user_agent,
                                int timeout_ms) {
  return https_get_ex(url, make_opts(user_agent, timeout_ms));
}

Result<HttpResponse> https_post(const std::string& url,
                                 const std::vector<std::uint8_t>& body,
                                 const std::string& content_type,
                                 const std::string& user_agent) {
  auto opts = make_opts(user_agent, 10000);
  return https_post_ex(url, body, opts, content_type);
}

}  // namespace real::net
