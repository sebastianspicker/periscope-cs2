// c2_client.cpp — Full C2 protocol: framed ACL2 messages, ChaCha20 + HMAC,
// TCP and HTTPS transports, auth handshake, beacon, failover reconnect.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#else
#  include <unistd.h>
#endif

namespace real::net {
namespace {

bool is_loopback_endpoint(const std::string& endpoint) {
  std::string host = parse_url(endpoint).host;
  std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return host == "localhost" || host == "127.0.0.1" || host == "::1";
}

bool has_auth_token(const C2Config& config, const std::string& auth_token) {
  return !auth_token.empty() || !config.auth_token.empty();
}

}  // namespace

// Frame layout (little-endian):
//   magic u32 | type u8 | flags u8 | seq u32 | payload_len u32 | payload | mac[32]?
// When Encrypted: payload = chacha20_seal(session_key, plain)
// When HasMac: trailing 32-byte HMAC-SHA256 over all prior bytes with session_key

Result<std::vector<std::uint8_t>> c2_pack_frame(
    const C2Message& msg, const std::vector<std::uint8_t>& session_key) {
  std::vector<std::uint8_t> payload = msg.payload;
  std::uint8_t flags = msg.flags;

  if (!session_key.empty() && (flags & static_cast<std::uint8_t>(C2Flags::Encrypted))) {
    if (session_key.size() != 32)
      return Result<std::vector<std::uint8_t>>({}, "session key must be 32 bytes");
    auto sealed = chacha20_seal(session_key, payload.data(), payload.size());
    if (!sealed) return Result<std::vector<std::uint8_t>>({}, sealed.error_msg);
    payload = std::move(*sealed);
  }

  const bool want_mac = !session_key.empty() &&
      ((flags & static_cast<std::uint8_t>(C2Flags::HasMac)) ||
       (flags & static_cast<std::uint8_t>(C2Flags::Encrypted)));
  if (want_mac) flags = static_cast<std::uint8_t>(flags | static_cast<std::uint8_t>(C2Flags::HasMac));

  std::vector<std::uint8_t> frame;
  frame.resize(4 + 1 + 1 + 4 + 4);
  write_u32_le(frame.data(), kC2FrameMagic);
  frame[4] = static_cast<std::uint8_t>(msg.type);
  frame[5] = flags;
  write_u32_le(frame.data() + 6, msg.seq);
  write_u32_le(frame.data() + 10, static_cast<std::uint32_t>(payload.size()));
  frame.insert(frame.end(), payload.begin(), payload.end());

  if (want_mac) {
    auto mac = hmac_sha256(session_key.data(), session_key.size(),
                           frame.data(), frame.size());
    frame.insert(frame.end(), mac.begin(), mac.end());
  }
  return frame;
}

Result<C2Message> c2_unpack_frame(
    const std::uint8_t* data, std::size_t len, std::size_t* consumed,
    const std::vector<std::uint8_t>& session_key) {
  if (consumed) *consumed = 0;
  if (!data || len < 14)
    return Result<C2Message>({}, "Frame too short");

  const std::uint32_t magic = read_u32_le(data);
  if (magic != kC2FrameMagic)
    return Result<C2Message>({}, "Bad C2 frame magic");

  C2Message msg;
  msg.type = static_cast<C2MessageType>(data[4]);
  msg.flags = data[5];
  msg.seq = read_u32_le(data + 6);
  const std::uint32_t plen = read_u32_le(data + 10);
  const bool has_mac = (msg.flags & static_cast<std::uint8_t>(C2Flags::HasMac)) != 0;
  const std::size_t total = 14 + plen + (has_mac ? 32u : 0u);
  if (len < total)
    return Result<C2Message>({}, "Incomplete C2 frame");

  if (has_mac) {
    if (session_key.size() != 32)
      return Result<C2Message>({}, "MAC present but no session key");
    auto mac = hmac_sha256(session_key.data(), session_key.size(), data, 14 + plen);
    if (mac.size() != 32 ||
        std::memcmp(mac.data(), data + 14 + plen, 32) != 0)
      return Result<C2Message>({}, "C2 frame MAC mismatch");
  }

  std::vector<std::uint8_t> payload(data + 14, data + 14 + plen);
  if ((msg.flags & static_cast<std::uint8_t>(C2Flags::Encrypted)) &&
      !session_key.empty()) {
    auto opened = chacha20_open(session_key, payload.data(), payload.size());
    if (!opened) return Result<C2Message>({}, opened.error_msg);
    payload = std::move(*opened);
  }
  msg.payload = std::move(payload);
  if (consumed) *consumed = total;
  return msg;
}

// ── C2Client ───────────────────────────────────────────────────────

C2Client::C2Client(const std::string& endpoint, const std::string& auth_token)
    : endpoint_(endpoint), auth_token_(auth_token) {
  config_.endpoint = endpoint;
  config_.auth_token = auth_token;
  auto p = parse_url(endpoint);
  if (p.scheme == "http" || p.scheme == "https")
    config_.transport = C2Config::Transport::HttpsPost;
  else
    config_.transport = C2Config::Transport::TcpFramed;
}

C2Client::C2Client(const C2Config& config)
    : config_(config),
      endpoint_(config.endpoint),
      auth_token_(config.auth_token) {}

C2Client::~C2Client() { disconnect(); }

Result<void> C2Client::perform_auth_handshake() {
  if (auth_token_.empty() && config_.auth_token.empty()) {
    const std::string endpoint = active_endpoint_.empty() ? endpoint_ : active_endpoint_;
    if (!config_.allow_loopback_test_without_auth || !is_loopback_endpoint(endpoint)) {
      return Result<void>("C2 authentication token is required for non-test endpoints");
    }
    // Unauthenticated sessions are restricted to an explicit loopback test
    // endpoint and receive an endpoint-bound key.
    session_key_ = derive_session_key("loopback-test:" + endpoint, "ac-lab-c2-loopback");
    return Result<void>();
  }
  const std::string token = auth_token_.empty() ? config_.auth_token : auth_token_;
  session_key_ = derive_session_key(token, "ac-lab-c2");

  C2Message auth;
  auth.type = C2MessageType::AuthResponse;
  auth.seq = next_seq_++;
  auth.flags = static_cast<std::uint8_t>(C2Flags::Encrypted) |
               static_cast<std::uint8_t>(C2Flags::HasMac);
  // payload: token hash proof (never send raw token after key derivation)
  auto proof = hmac_sha256(session_key_,
                           std::vector<std::uint8_t>(token.begin(), token.end()));
  auth.payload = proof;

  // On TCP we send; server may not exist — for connect() we only prepare key.
  // Actual auth message is sent when send_message is used, or here if connected.
  if (connected_ && config_.transport == C2Config::Transport::TcpFramed && sock_.fd >= 0) {
    auto packed = c2_pack_frame(auth, session_key_);
    if (!packed) return Result<void>(packed.error_msg);
    // Best-effort; remote may not speak ACL2 yet.
    send_all(sock_, packed->data(), static_cast<int>(packed->size()));
  }
  return Result<void>();
}

Result<void> C2Client::connect_tcp(const std::string& endpoint) {
  auto parsed = parse_url(endpoint);
  std::string host = parsed.host;
  std::uint16_t port = parsed.port;
  if (host.empty()) {
    // bare host:port
    const auto colon = endpoint.rfind(':');
    if (colon != std::string::npos) {
      host = endpoint.substr(0, colon);
      try {
        port = static_cast<std::uint16_t>(std::stoi(endpoint.substr(colon + 1)));
      } catch (...) {
        return Result<void>("Bad endpoint port");
      }
    } else {
      host = endpoint;
      port = 443;
    }
  }
  // Default bare port for tcp scheme was 80 — C2 often uses custom ports from URL.
  if (parsed.scheme == "tcp" && parsed.port == 80 && endpoint.find(':') != std::string::npos) {
    // already handled
  }

  ConnectOptions opts;
  opts.timeout_ms = config_.connect_timeout_ms;
  auto sock = connect_ex(host, port, opts, Protocol::Tcp);
  if (!sock) return Result<void>(sock.error_msg);
  sock_ = *sock;
  connected_ = true;
  active_endpoint_ = endpoint;
  next_seq_ = 1;
  auto auth = perform_auth_handshake();
  if (!auth) disconnect();
  return auth;
}

Result<void> C2Client::connect_https(const std::string& endpoint) {
  // HTTPS channel: validate reachability with a lightweight GET/OPTIONS or just
  // mark session ready and derive keys. Actual messages go through https_post.
  active_endpoint_ = endpoint;
  connected_ = true;
  next_seq_ = 1;
  auto auth = perform_auth_handshake();
  if (!auth) {
    connected_ = false;
    return auth;
  }
  return Result<void>();
}

Result<void> C2Client::connect() {
  if (connected_) return Result<void>();

  std::vector<std::string> endpoints;
  endpoints.push_back(config_.endpoint.empty() ? endpoint_ : config_.endpoint);
  for (const auto& e : config_.failover_endpoints) endpoints.push_back(e);

  FixedError last_err("No endpoints");
  for (const auto& ep : endpoints) {
    if (ep.empty()) continue;
    if (!has_auth_token(config_, auth_token_) &&
        (!config_.allow_loopback_test_without_auth || !is_loopback_endpoint(ep))) {
      last_err = FixedError("C2 authentication token is required for non-test endpoints");
      continue;
    }
    Result<void> r;
    if (config_.transport == C2Config::Transport::HttpsPost ||
        parse_url(ep).scheme == "http" || parse_url(ep).scheme == "https") {
      config_.transport = C2Config::Transport::HttpsPost;
      r = connect_https(ep);
    } else {
      config_.transport = C2Config::Transport::TcpFramed;
      r = connect_tcp(ep);
    }
    if (r) return r;
    last_err = r.error_msg;
  }
  return Result<void>(last_err);
}

Result<C2Message> C2Client::send_tcp(const C2Message& msg, int timeout_ms) {
  if (!connected_ || sock_.fd < 0)
    return Result<C2Message>({}, "Not connected");
  set_timeout(sock_, timeout_ms);

  C2Message out = msg;
  if (out.seq == 0) out.seq = next_seq_++;
  if (config_.encrypt_payloads && !session_key_.empty()) {
    out.flags = static_cast<std::uint8_t>(out.flags |
        static_cast<std::uint8_t>(C2Flags::Encrypted) |
        static_cast<std::uint8_t>(C2Flags::HasMac));
  }

  auto packed = c2_pack_frame(out, session_key_);
  if (!packed) return Result<C2Message>({}, packed.error_msg);
  auto sent = send_all(sock_, packed->data(), static_cast<int>(packed->size()));
  if (!sent) {
    disconnect();
    return Result<C2Message>({}, sent.error_msg);
  }

  // Read response frame: need at least header then remainder
  std::uint8_t hdr[14];
  auto rh = recv_exact(sock_, hdr, 14);
  if (!rh) {
    disconnect();
    return Result<C2Message>({}, rh.error_msg);
  }
  const std::uint32_t plen = read_u32_le(hdr + 10);
  const bool has_mac = (hdr[5] & static_cast<std::uint8_t>(C2Flags::HasMac)) != 0;
  if (plen > 16 * 1024 * 1024) {
    disconnect();
    return Result<C2Message>({}, "C2 payload too large");
  }
  std::vector<std::uint8_t> frame(hdr, hdr + 14);
  frame.resize(14 + plen + (has_mac ? 32u : 0u));
  if (plen + (has_mac ? 32u : 0u) > 0) {
    auto rb = recv_exact(sock_, frame.data() + 14,
                         static_cast<int>(plen + (has_mac ? 32u : 0u)));
    if (!rb) {
      disconnect();
      return Result<C2Message>({}, rb.error_msg);
    }
  }
  return c2_unpack_frame(frame.data(), frame.size(), nullptr, session_key_);
}

Result<C2Message> C2Client::send_https(const C2Message& msg, int timeout_ms) {
  if (!connected_) return Result<C2Message>({}, "Not connected");

  C2Message out = msg;
  if (out.seq == 0) out.seq = next_seq_++;
  if (config_.encrypt_payloads && !session_key_.empty()) {
    out.flags = static_cast<std::uint8_t>(out.flags |
        static_cast<std::uint8_t>(C2Flags::Encrypted) |
        static_cast<std::uint8_t>(C2Flags::HasMac));
  }
  auto packed = c2_pack_frame(out, session_key_);
  if (!packed) return Result<C2Message>({}, packed.error_msg);

  std::string url = active_endpoint_.empty() ? endpoint_ : active_endpoint_;
  // Ensure path
  auto p = parse_url(url);
  if (p.path.empty() || p.path == "/") {
    if (url.back() == '/') url += config_.https_path.substr(1);
    else url += config_.https_path;
  }

  HttpRequestOptions opts;
  opts.timeout_ms = timeout_ms;
  opts.user_agent = config_.user_agent;
  opts.headers["X-C2-Seq"] = std::to_string(out.seq);
  if (!auth_token_.empty())
    opts.headers["Authorization"] = "Bearer " + auth_token_;

  auto resp = https_post_ex(url, *packed, opts, "application/octet-stream");
  if (!resp) return Result<C2Message>({}, resp.error_msg);
  if (resp->status_code < 200 || resp->status_code >= 300)
    return Result<C2Message>({},
        "HTTPS C2 status " + std::to_string(resp->status_code));
  if (resp->body.empty()) {
    C2Message ack;
    ack.type = C2MessageType::Ack;
    ack.seq = out.seq;
    return ack;
  }
  return c2_unpack_frame(resp->body.data(), resp->body.size(), nullptr, session_key_);
}

Result<C2Message> C2Client::send_message(const C2Message& msg, int timeout_ms) {
  if (!connected_) return Result<C2Message>({}, "Not connected");
  if (config_.transport == C2Config::Transport::HttpsPost)
    return send_https(msg, timeout_ms);
  return send_tcp(msg, timeout_ms);
}

Result<std::vector<std::uint8_t>> C2Client::fetch_offsets(
    const std::string& game_version) {
  C2Message req;
  req.type = C2MessageType::OffsetRequest;
  req.seq = next_seq_++;
  req.payload.assign(game_version.begin(), game_version.end());
  auto resp = send_message(req);
  if (!resp) return Result<std::vector<std::uint8_t>>({}, resp.error_msg);
  return resp->payload;
}

Result<void> C2Client::upload_radar_data(const std::vector<std::uint8_t>& data) {
  C2Message req;
  req.type = C2MessageType::RadarDataUpload;
  req.seq = next_seq_++;
  req.payload = data;
  auto resp = send_message(req);
  if (!resp) return Result<void>(resp.error_msg);
  return Result<void>();
}

Result<C2Message> C2Client::heartbeat() {
  C2Message req;
  req.type = C2MessageType::Heartbeat;
  req.seq = next_seq_++;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  std::uint8_t ts[8];
  write_u64_le(ts, static_cast<std::uint64_t>(ms));
  req.payload.assign(ts, ts + 8);
  return send_message(req);
}

void C2Client::disconnect() {
  if (sock_.fd >= 0) {
    close(sock_);
    sock_.fd = -1;
  }
  connected_ = false;
  if (!session_key_.empty()) {
    secure_zero(session_key_.data(), session_key_.size());
    session_key_.clear();
  }
}

Result<void> C2Client::reconnect(int max_attempts) {
  disconnect();
  int delay = 200;
  FixedError last("reconnect failed");
  for (int i = 0; i < max_attempts; ++i) {
    auto r = connect();
    if (r) return r;
    last = r.error_msg;
    // crude sleep via busy-wait free: use socket timeout trick — platform sleep
#if LR_PLATFORM_WINDOWS
    Sleep(static_cast<DWORD>(jitter_ms(delay, 20)));
#else
    usleep(static_cast<useconds_t>(jitter_ms(delay, 20) * 1000));
#endif
    delay = (std::min)(delay * 2, 5000);
  }
  return Result<void>(last);
}

// ── BeaconScheduler ────────────────────────────────────────────────

std::uint64_t BeaconScheduler::now_ms() {
#if LR_PLATFORM_WINDOWS
  return GetTickCount64();
#else
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

BeaconScheduler::BeaconScheduler(int interval_ms, int jitter_percent)
    : interval_ms_(interval_ms), jitter_percent_(jitter_percent) {
  reset();
}

void BeaconScheduler::set_interval(int interval_ms, int jitter_percent) {
  interval_ms_ = interval_ms;
  jitter_percent_ = jitter_percent;
  reset();
}

void BeaconScheduler::reset() {
  next_fire_ms_ = now_ms() + static_cast<std::uint64_t>(
      jitter_ms(interval_ms_ > 0 ? interval_ms_ : 1, jitter_percent_));
}

bool BeaconScheduler::should_fire() {
  if (interval_ms_ <= 0) return false;
  const auto n = now_ms();
  if (n < next_fire_ms_) return false;
  ++fire_count_;
  next_fire_ms_ = n + static_cast<std::uint64_t>(
      jitter_ms(interval_ms_, jitter_percent_));
  return true;
}

int BeaconScheduler::ms_until_fire() const {
  const auto n = now_ms();
  if (n >= next_fire_ms_) return 0;
  return static_cast<int>(next_fire_ms_ - n);
}

}  // namespace real::net
