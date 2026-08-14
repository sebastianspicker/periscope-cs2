// net_client.hpp — State-of-the-art real networking backends for C2,
// offset fetching, radar SaaS, DNS tunneling, proxies, WebSocket, and IPC.
//
// Educational security-research stack: full implementations (no stubs).
// Windows path uses WinHTTP (TLS 1.2+) for HTTPS and Winsock for sockets.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/net/net_crypto.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace real::net {

// ── Socket Abstraction ─────────────────────────────────────────────

/// Socket handle (opaque native descriptor).
struct Socket {
  int fd = -1;  // native socket descriptor, -1 = invalid
  bool operator==(const Socket& o) const { return fd == o.fd; }
  bool operator!=(const Socket& o) const { return fd != o.fd; }
  explicit operator bool() const { return fd >= 0; }
};

/// Protocol type.
enum class Protocol {
  Tcp,
  Udp,
};

/// Connect options for hardened / lab-friendly sockets.
struct ConnectOptions {
  int timeout_ms = 10000;
  bool nodelay = true;
  bool keepalive = true;
  /// Optional SOCKS5 or HTTP CONNECT proxy host:port (empty = direct).
  std::string proxy_host;
  std::uint16_t proxy_port = 0;
  enum class ProxyType { None, Socks5, HttpConnect } proxy_type = ProxyType::None;
  std::string proxy_user;
  std::string proxy_pass;
};

/// Connect to a remote host (getaddrinfo + TCP/UDP).
Result<Socket> connect(const std::string& host, std::uint16_t port,
                        Protocol proto = Protocol::Tcp);

/// Connect with options (timeout, Nagle, keepalive, proxy).
Result<Socket> connect_ex(const std::string& host, std::uint16_t port,
                           const ConnectOptions& opts,
                           Protocol proto = Protocol::Tcp);

/// Bind and listen on TCP (lab server / self-test).
Result<Socket> listen_tcp(const std::string& host, std::uint16_t port,
                           int backlog = 16);

/// Accept one client from a listening socket.
Result<Socket> accept(Socket listener, int timeout_ms = -1);

/// Send data on a socket (may be partial).
Result<int> send(Socket sock, const std::uint8_t* data, int size);

/// Send all bytes (loops until complete or error).
Result<void> send_all(Socket sock, const std::uint8_t* data, int size);

/// Receive data from a socket.
Result<int> recv(Socket sock, std::uint8_t* buffer, int buffer_size);

/// Receive exactly `size` bytes (or error / peer close).
Result<void> recv_exact(Socket sock, std::uint8_t* buffer, int size);

/// UDP sendto / recvfrom.
Result<int> sendto(Socket sock, const std::uint8_t* data, int size,
                    const std::string& host, std::uint16_t port);
Result<int> recvfrom(Socket sock, std::uint8_t* buffer, int buffer_size,
                      std::string* out_host = nullptr,
                      std::uint16_t* out_port = nullptr);

/// Close a socket.
Result<void> close(Socket sock);

/// Set socket timeout (send + recv).
Result<void> set_timeout(Socket sock, int timeout_ms);

/// TCP_NODELAY.
Result<void> set_nodelay(Socket sock, bool enabled);

/// SO_KEEPALIVE.
Result<void> set_keepalive(Socket sock, bool enabled);

// ── TLS / HTTPS Client ─────────────────────────────────────────────

/// HTTPS / HTTP response with full header map.
struct HttpResponse {
  int status_code = 0;
  std::vector<std::uint8_t> body;
  std::string content_type;
  std::map<std::string, std::string> headers;
  /// Final URL after redirects (if any).
  std::string final_url;
};

/// Request options for https_get / https_post.
struct HttpRequestOptions {
  std::string user_agent = "EducationalLab/2.0";
  int timeout_ms = 10000;
  std::map<std::string, std::string> headers;
  bool follow_redirects = true;
  int max_redirects = 5;
  /// Optional SHA-256 hex of peer cert (pin). Empty = no pin.
  std::string cert_pin_sha256_hex;
  /// Force insecure (skip cert validation) — lab only.
  bool insecure_skip_verify = false;
  /// Optional proxy URL host:port.
  std::string proxy;
  /// Domain-fronting style: send this Host header instead of URL host.
  std::string front_host;
};

/// Perform an HTTPS/HTTP GET (WinHTTP TLS on Windows; raw HTTP fallback).
Result<HttpResponse> https_get(const std::string& url,
                                const std::string& user_agent = {},
                                int timeout_ms = 10000);

/// GET with full options.
Result<HttpResponse> https_get_ex(const std::string& url,
                                   const HttpRequestOptions& opts);

/// Perform an HTTPS/HTTP POST.
Result<HttpResponse> https_post(const std::string& url,
                                 const std::vector<std::uint8_t>& body,
                                 const std::string& content_type = {},
                                 const std::string& user_agent = {});

/// POST with full options.
Result<HttpResponse> https_post_ex(const std::string& url,
                                    const std::vector<std::uint8_t>& body,
                                    const HttpRequestOptions& opts,
                                    const std::string& content_type = {});

// ── C2 Protocol ────────────────────────────────────────────────────

/// C2 message types.
enum class C2MessageType : std::uint8_t {
  Heartbeat = 0,
  OffsetRequest = 1,
  OffsetResponse = 2,
  ConfigUpdate = 3,
  RadarDataUpload = 4,
  Command = 5,
  AuthChallenge = 6,
  AuthResponse = 7,
  Beacon = 8,
  Ack = 0xFF,
};

/// Wire frame flags.
enum class C2Flags : std::uint8_t {
  None = 0,
  Encrypted = 1 << 0,
  HasMac = 1 << 1,
  Compressed = 1 << 2,
};

/// A single C2 protocol message.
struct C2Message {
  C2MessageType type = C2MessageType::Heartbeat;
  std::uint32_t seq = 0;
  std::vector<std::uint8_t> payload;
  std::uint8_t flags = 0;
};

/// Frame magic bytes for lab C2: "ACL2"
constexpr std::uint32_t kC2FrameMagic = 0x324C4341u;  // 'ACL2' LE

/// Pack a C2 message into a length-prefixed authenticated frame.
/// When session_key (32 bytes) is non-empty, payload is ChaCha20-sealed and
/// the frame is HMAC-SHA256 tagged.
Result<std::vector<std::uint8_t>> c2_pack_frame(
    const C2Message& msg, const std::vector<std::uint8_t>& session_key = {});

/// Unpack a C2 frame (consumes one frame from buffer prefix).
Result<C2Message> c2_unpack_frame(
    const std::uint8_t* data, std::size_t len, std::size_t* consumed = nullptr,
    const std::vector<std::uint8_t>& session_key = {});

/// C2 client configuration.
struct C2Config {
  std::string endpoint;          // e.g. http://127.0.0.1:8443/c2 or host:port
  std::string auth_token;
  std::vector<std::string> failover_endpoints;
  int connect_timeout_ms = 5000;
  int io_timeout_ms = 5000;
  int beacon_interval_ms = 30000;
  int beacon_jitter_percent = 25;
  bool encrypt_payloads = true;
  /// Transport: raw TCP framed, or HTTPS POST channel.
  enum class Transport { TcpFramed, HttpsPost } transport = Transport::TcpFramed;
  std::string https_path = "/api/c2";
  std::string user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";
};

/// C2 client that communicates with a remote command server.
class C2Client {
public:
  explicit C2Client(const std::string& endpoint,
                    const std::string& auth_token = {});
  explicit C2Client(const C2Config& config);
  ~C2Client();

  C2Client(const C2Client&) = delete;
  C2Client& operator=(const C2Client&) = delete;

  /// Connect to the C2 server (TCP framed or HTTPS session init).
  Result<void> connect();

  /// Send a message and wait for acknowledgment / response.
  Result<C2Message> send_message(const C2Message& msg, int timeout_ms = 5000);

  /// Fetch encrypted offset data from C2.
  Result<std::vector<std::uint8_t>> fetch_offsets(const std::string& game_version);

  /// Upload radar data to C2 (multi-box setup).
  Result<void> upload_radar_data(const std::vector<std::uint8_t>& data);

  /// Send a heartbeat / beacon.
  Result<C2Message> heartbeat();

  /// Check if connected.
  bool is_connected() const { return connected_; }

  /// Session key derived after auth (empty if plaintext mode).
  const std::vector<std::uint8_t>& session_key() const { return session_key_; }

  /// Disconnect and wipe session material.
  void disconnect();

  /// Reconnect with exponential backoff (attempts).
  Result<void> reconnect(int max_attempts = 3);

private:
  Result<void> connect_tcp(const std::string& endpoint);
  Result<void> connect_https(const std::string& endpoint);
  Result<C2Message> send_tcp(const C2Message& msg, int timeout_ms);
  Result<C2Message> send_https(const C2Message& msg, int timeout_ms);
  Result<void> perform_auth_handshake();

  C2Config config_;
  std::string endpoint_;
  std::string auth_token_;
  bool connected_ = false;
  Socket sock_;
  std::uint32_t next_seq_ = 1;
  std::vector<std::uint8_t> session_key_;
  std::string active_endpoint_;
};

// ── Beacon scheduler ───────────────────────────────────────────────

/// Stateful beacon timer with jitter (no threads — call poll()).
class BeaconScheduler {
public:
  explicit BeaconScheduler(int interval_ms = 30000, int jitter_percent = 25);

  void set_interval(int interval_ms, int jitter_percent = 25);
  void reset();

  /// True when it is time to fire a beacon; advances next fire time.
  bool should_fire();

  /// Milliseconds until next fire (0 if due).
  int ms_until_fire() const;

  std::uint64_t fire_count() const { return fire_count_; }

private:
  int interval_ms_ = 30000;
  int jitter_percent_ = 25;
  std::uint64_t next_fire_ms_ = 0;
  std::uint64_t fire_count_ = 0;
  static std::uint64_t now_ms();
};

// ── Named Pipe / IPC ───────────────────────────────────────────────

/// Named pipe abstraction for local IPC (holder ↔ reader ↔ UI).
class NamedPipe {
public:
  NamedPipe() = default;
  ~NamedPipe();

  NamedPipe(const NamedPipe&) = delete;
  NamedPipe& operator=(const NamedPipe&) = delete;

  /// Create a named pipe server (blocks until client connects unless async note).
  Result<void> create_server(const std::string& pipe_name);

  /// Create server without waiting for client (Windows: returns after CreateNamedPipe).
  Result<void> create_server_nowait(const std::string& pipe_name);

  /// Wait for a client on a server pipe created with create_server_nowait.
  Result<void> wait_for_client(int timeout_ms = -1);

  /// Connect to an existing named pipe server.
  Result<void> connect_client(const std::string& pipe_name);

  /// Connect with timeout.
  Result<void> connect_client(const std::string& pipe_name, int timeout_ms);

  /// Write data to the pipe.
  Result<int> write(const std::uint8_t* data, int size);

  /// Read data from the pipe.
  Result<int> read(std::uint8_t* buffer, int buffer_size);

  /// Length-prefixed framed write (u32 LE length + payload).
  Result<void> write_frame(const std::uint8_t* data, int size);

  /// Length-prefixed framed read.
  Result<std::vector<std::uint8_t>> read_frame(int max_size = 16 * 1024 * 1024);

  /// Close the pipe.
  void close();

  bool is_open() const { return handle_ != -1; }

private:
  std::intptr_t handle_ = -1;
  bool is_server_ = false;
};

// ── DNS Tunneling / Out-of-Band C2 ─────────────────────────────────

/// DNS record types.
enum class DnsType : std::uint16_t {
  A = 1,
  NS = 2,
  CNAME = 5,
  TXT = 16,
  AAAA = 28,
};

struct DnsRecord {
  std::string name;
  DnsType type = DnsType::A;
  std::uint32_t ttl = 0;
  std::string data;  // A: dotted IPv4; TXT: text; etc.
};

/// Encode data as DNS labels for exfiltration (base32, max 63/label).
Result<std::vector<std::string>> encode_dns_tunnel(
    const std::uint8_t* data, std::size_t size,
    const std::string& domain);

/// Decode data extracted from DNS labels / TXT responses.
Result<std::vector<std::uint8_t>> decode_dns_tunnel(
    const std::vector<std::string>& txt_records);

/// Build a raw DNS query packet (UDP payload).
Result<std::vector<std::uint8_t>> dns_build_query(
    const std::string& qname, DnsType type = DnsType::A,
    std::uint16_t id = 0);

/// Parse a DNS response packet into records.
Result<std::vector<DnsRecord>> dns_parse_response(
    const std::uint8_t* data, std::size_t len);

/// Perform a DNS query via UDP (default system resolver 8.8.8.8 or custom).
Result<std::vector<DnsRecord>> dns_query(
    const std::string& name, DnsType type = DnsType::A,
    const std::string& resolver = "8.8.8.8", std::uint16_t port = 53,
    int timeout_ms = 3000);

/// DNS-over-HTTPS (RFC 8484) GET to a DoH endpoint.
Result<std::vector<DnsRecord>> dns_query_doh(
    const std::string& name, DnsType type = DnsType::A,
    const std::string& doh_url = "https://cloudflare-dns.com/dns-query",
    int timeout_ms = 5000);

// ── SOCKS5 / HTTP CONNECT proxy ────────────────────────────────────

/// Establish SOCKS5 proxy tunnel; returns connected socket to target.
Result<Socket> socks5_connect(const std::string& proxy_host, std::uint16_t proxy_port,
                               const std::string& target_host, std::uint16_t target_port,
                               const std::string& user = {},
                               const std::string& pass = {},
                               int timeout_ms = 10000);

/// Establish HTTP CONNECT tunnel.
Result<Socket> http_connect_proxy(const std::string& proxy_host, std::uint16_t proxy_port,
                                   const std::string& target_host, std::uint16_t target_port,
                                   int timeout_ms = 10000);

// ── WebSocket client ───────────────────────────────────────────────

class WebSocketClient {
public:
  WebSocketClient() = default;
  ~WebSocketClient();

  WebSocketClient(const WebSocketClient&) = delete;
  WebSocketClient& operator=(const WebSocketClient&) = delete;

  /// Connect to ws:// or wss:// URL (wss uses WinHTTP upgrade where available;
  /// ws uses raw TCP + HTTP upgrade).
  Result<void> connect(const std::string& url, int timeout_ms = 10000);

  Result<void> send_text(const std::string& text);
  Result<void> send_binary(const std::uint8_t* data, std::size_t len);
  Result<std::vector<std::uint8_t>> recv_message(int timeout_ms = 10000);

  void close();
  bool is_open() const { return open_; }

private:
  Socket sock_;
  bool open_ = false;
  bool is_tls_ = false;
  std::string host_;
  std::string path_;
};

// ── Offset CDN / SaaS API ──────────────────────────────────────────

/// Fetch CS2 offsets from a CDN/schema SaaS endpoint.
Result<std::vector<std::uint8_t>> fetch_schema_offsets(
    const std::string& saas_url, const std::string& api_key = {});

/// Fetch an encrypted offset blob from a remote C2 and XOR/ChaCha-open it.
Result<std::vector<std::uint8_t>> fetch_encrypted_offsets(
    const std::string& c2_url, const std::vector<std::uint8_t>& key);

/// Build a verified offset blob for lab self-test (inverse of fetch_offsets parser).
std::vector<std::uint8_t> build_offset_blob(
    std::uint32_t cs2_version,
    const std::uint64_t offsets[10],
    std::uint8_t xor_key = 0x5A);

// ── URL parse (public for tests / callers) ─────────────────────────

struct ParsedUrl {
  std::string scheme;  // "http", "https", "ws", "wss", ""
  std::string host;
  std::string path;
  std::uint16_t port = 80;
  bool is_https = false;
  bool is_ws = false;
};

ParsedUrl parse_url(const std::string& url);

}  // namespace real::net
