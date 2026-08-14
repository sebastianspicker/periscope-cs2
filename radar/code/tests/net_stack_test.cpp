// net_stack_test.cpp — Drives shipped real::net entry points on the real path.
// Loopback TCP/HTTP server, crypto round-trips, DNS tunnel, C2 framing,
// offset blob verify, beacon scheduler, named-pipe frames (Windows).

#include "real/net/net_client.hpp"
#include "real/net/net_crypto.hpp"
#include "real/net/offset_fetch.hpp"
#include "real/platform.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static int g_fails = 0;
static int g_passes = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("OK: %s\n", msg);
    ++g_passes;
  }
}

// ── URL parse ──────────────────────────────────────────────────────

static void test_parse_url() {
  std::printf("\n=== parse_url ===\n");
  auto a = real::net::parse_url("https://cdn.example.com:8443/v1/offsets?x=1");
  expect(a.is_https, "https scheme sets is_https");
  expect(a.host == "cdn.example.com", "host parsed");
  expect(a.port == 8443, "port parsed");
  expect(a.path.find("/v1/offsets") == 0, "path parsed");

  auto b = real::net::parse_url("http://127.0.0.1/health");
  expect(!b.is_https && b.port == 80, "http default port 80");
  expect(b.host == "127.0.0.1", "ipv4 host");
  expect(b.path == "/health", "path /health");

  auto c = real::net::parse_url("ws://localhost:9001/socket");
  expect(c.is_ws, "ws sets is_ws");
  expect(c.port == 9001, "ws port");
}

// ── Crypto ─────────────────────────────────────────────────────────

static void test_crypto() {
  std::printf("\n=== crypto ===\n");
  using namespace real::net;

  // SHA-256 empty string — known test vector
  auto h = sha256(std::string(""));
  expect(h.size() == 32, "sha256 size 32");
  expect(to_hex(h) ==
             "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
         "sha256 empty vector");

  const char* msg = "ac-lab-net-stack";
  auto h2 = sha256(reinterpret_cast<const std::uint8_t*>(msg), std::strlen(msg));
  auto h3 = sha256(std::string(msg));
  expect(h2 == h3, "sha256 pointer vs string");

  // HMAC deterministic
  std::vector<std::uint8_t> key = {1, 2, 3, 4, 5, 6, 7, 8};
  auto m1 = hmac_sha256(key, std::vector<std::uint8_t>(msg, msg + std::strlen(msg)));
  auto m2 = hmac_sha256(key, std::vector<std::uint8_t>(msg, msg + std::strlen(msg)));
  expect(m1 == m2 && m1.size() == 32, "hmac deterministic 32");

  // ChaCha20 seal/open round-trip
  auto sk = derive_session_key("test-token", "salt-v1");
  expect(sk.size() == 32, "session key 32 bytes");
  const std::uint8_t plain[] = {0x10, 0x20, 0x30, 0x40, 0x50, 'R', 'A', 'D', 'A', 'R'};
  auto sealed = chacha20_seal(sk, plain, sizeof(plain));
  expect(static_cast<bool>(sealed), "chacha20_seal ok");
  expect(sealed && sealed->size() == 12 + sizeof(plain), "sealed = nonce+ct");
  auto opened = chacha20_open(sk, sealed->data(), sealed->size());
  expect(static_cast<bool>(opened), "chacha20_open ok");
  expect(opened && opened->size() == sizeof(plain) &&
             std::memcmp(opened->data(), plain, sizeof(plain)) == 0,
         "chacha20 round-trip");

  // Base32/hex/base64
  auto b32 = to_base32(plain, sizeof(plain));
  auto back = from_base32(b32);
  expect(static_cast<bool>(back) && back->size() == sizeof(plain) &&
             std::memcmp(back->data(), plain, sizeof(plain)) == 0,
         "base32 round-trip");

  auto hx = to_hex(plain, sizeof(plain));
  auto hxback = from_hex(hx);
  expect(static_cast<bool>(hxback) && *hxback == std::vector<std::uint8_t>(plain, plain + sizeof(plain)),
         "hex round-trip");

  auto b64 = to_base64(plain, sizeof(plain));
  auto b64back = from_base64(b64);
  expect(static_cast<bool>(b64back) && b64back->size() == sizeof(plain), "base64 round-trip");

  // CRC32
  const std::uint8_t one[] = {'1'};
  expect(crc32(one, 1) != 0, "crc32 non-trivial");

  // SecureBuffer
  SecureBuffer sec(plain, sizeof(plain));
  expect(!sec.empty() && sec.size() == sizeof(plain), "SecureBuffer size");
  expect(std::memcmp(sec.get(), plain, sizeof(plain)) == 0, "SecureBuffer decrypt");

  // random + jitter
  auto rb = random_bytes(16);
  expect(static_cast<bool>(rb) && rb->size() == 16, "random_bytes 16");
  const int j = jitter_ms(1000, 20);
  expect(j >= 800 && j <= 1200, "jitter_ms within ±20%");
}

// ── DNS tunnel ─────────────────────────────────────────────────────

static void test_dns_tunnel() {
  std::printf("\n=== dns tunnel ===\n");
  using namespace real::net;
  const std::uint8_t payload[] = "offset-blob-v2-secret";
  auto chunks = encode_dns_tunnel(payload, sizeof(payload) - 1, "lab.example.com");
  expect(static_cast<bool>(chunks) && !chunks->empty(), "encode_dns_tunnel produces chunks");
  expect(chunks && chunks->front().find("lab.example.com") != std::string::npos,
         "domain appears in qname");

  auto decoded = decode_dns_tunnel(*chunks);
  expect(static_cast<bool>(decoded), "decode_dns_tunnel ok");
  expect(decoded && decoded->size() == sizeof(payload) - 1 &&
             std::memcmp(decoded->data(), payload, sizeof(payload) - 1) == 0,
         "dns tunnel round-trip");

  // Wire format build/parse loopback
  auto q = dns_build_query("example.com", DnsType::A, 0x1234);
  expect(static_cast<bool>(q) && q->size() > 12, "dns_build_query");
  expect(read_u16_be(q->data()) == 0x1234, "dns query id");
}

// ── C2 framing ─────────────────────────────────────────────────────

static void test_c2_framing() {
  std::printf("\n=== c2 framing ===\n");
  using namespace real::net;
  auto key = derive_session_key("frame-test", "ac-lab-c2");

  C2Message msg;
  msg.type = C2MessageType::OffsetRequest;
  msg.seq = 42;
  msg.flags = static_cast<std::uint8_t>(C2Flags::Encrypted) |
              static_cast<std::uint8_t>(C2Flags::HasMac);
  const char* ver = "1.40.5.1234";
  msg.payload.assign(ver, ver + std::strlen(ver));

  auto packed = c2_pack_frame(msg, key);
  expect(static_cast<bool>(packed) && packed->size() > 14, "c2_pack_frame");
  expect(read_u32_le(packed->data()) == kC2FrameMagic, "frame magic ACL2");

  std::size_t consumed = 0;
  auto unpacked = c2_unpack_frame(packed->data(), packed->size(), &consumed, key);
  expect(static_cast<bool>(unpacked), "c2_unpack_frame");
  expect(consumed == packed->size(), "consumed full frame");
  expect(unpacked && unpacked->type == C2MessageType::OffsetRequest, "type preserved");
  expect(unpacked && unpacked->seq == 42, "seq preserved");
  expect(unpacked && std::string(unpacked->payload.begin(), unpacked->payload.end()) == ver,
         "payload decrypted");

  // Tamper MAC
  auto bad = *packed;
  bad.back() ^= 0xFF;
  auto fail = c2_unpack_frame(bad.data(), bad.size(), nullptr, key);
  expect(!fail, "tampered frame rejected");
}

// ── Loopback TCP + raw HTTP ────────────────────────────────────────

static void test_loopback_http() {
  std::printf("\n=== loopback TCP/HTTP ===\n");
  using namespace real::net;

  auto listener = listen_tcp("127.0.0.1", 0, 4);
  // port 0 may not bind usefully on all stacks — pick ephemeral via bind on 0
  // Some platforms need explicit port; try a high port range.
  std::uint16_t port = 0;
  if (!listener) {
    for (std::uint16_t p = 38765; p < 38800; ++p) {
      listener = listen_tcp("127.0.0.1", p, 4);
      if (listener) {
        port = p;
        break;
      }
    }
  } else {
    // If listen_tcp(0) worked we still need the chosen port — re-listen fixed.
    close(*listener);
    port = 38777;
    listener = listen_tcp("127.0.0.1", port, 4);
  }
  expect(static_cast<bool>(listener), "listen_tcp");
  if (!listener) return;

  std::atomic<bool> server_ok{false};
  std::atomic<bool> server_done{false};
  std::thread server([&]() {
    auto client = accept(*listener, 5000);
    if (!client) {
      server_done = true;
      return;
    }
    // Read request
    std::uint8_t buf[4096];
    std::vector<std::uint8_t> req;
    set_timeout(*client, 3000);
    for (;;) {
      auto n = recv(*client, buf, sizeof(buf));
      if (!n || *n == 0) break;
      req.insert(req.end(), buf, buf + *n);
      if (std::string(reinterpret_cast<char*>(req.data()), req.size()).find("\r\n\r\n") !=
          std::string::npos)
        break;
    }
    const std::string body = "{\"ok\":true,\"svc\":\"net-lab\"}";
    const std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " +
        std::to_string(body.size()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;
    send_all(*client, reinterpret_cast<const std::uint8_t*>(resp.data()),
             static_cast<int>(resp.size()));
    close(*client);
    server_ok = true;
    server_done = true;
  });

  // Give server thread a moment
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/v1/health";
  auto resp = https_get(url, "NetStackTest/1.0", 5000);
  expect(static_cast<bool>(resp), "https_get loopback");
  if (resp) {
    expect(resp->status_code == 200, "status 200");
    const std::string body(resp->body.begin(), resp->body.end());
    expect(body.find("net-lab") != std::string::npos, "body contains net-lab");
    expect(resp->content_type.find("json") != std::string::npos ||
               resp->headers.count("Content-Type") > 0,
           "content-type set");
  }

  // Also exercise connect/send/recv primitives directly
  auto sock = connect("127.0.0.1", port, Protocol::Tcp);
  // Server already handled one client; start another accept cycle if needed.
  // Direct connect may fail if server thread finished — that's ok; HTTP path is primary.
  if (sock) {
    const char* ping = "PING";
    send_all(*sock, reinterpret_cast<const std::uint8_t*>(ping), 4);
    close(*sock);
  }

  server.join();
  close(*listener);
  expect(server_ok.load() || static_cast<bool>(resp), "server path exercised");
  (void)server_done;
}

// ── C2 over loopback TCP ───────────────────────────────────────────

static void test_c2_loopback() {
  std::printf("\n=== c2 loopback TCP ===\n");
  using namespace real::net;

  const std::uint16_t port = 38778;
  auto listener = listen_tcp("127.0.0.1", port, 4);
  if (!listener) {
    // port busy — try alternate
    expect(false, "listen c2 port");
    return;
  }

  auto key = derive_session_key("c2-lab-token", "ac-lab-c2");
  std::atomic<bool> ok{false};
  std::thread server([&]() {
    auto client = accept(*listener, 5000);
    if (!client) return;
    set_timeout(*client, 3000);
    // Auth frame (best-effort read)
    std::uint8_t hdr[14];
    if (!recv_exact(*client, hdr, 14)) {
      close(*client);
      return;
    }
    const std::uint32_t plen = read_u32_le(hdr + 10);
    const bool has_mac = (hdr[5] & static_cast<std::uint8_t>(C2Flags::HasMac)) != 0;
    std::vector<std::uint8_t> rest(plen + (has_mac ? 32u : 0u));
    if (!rest.empty()) recv_exact(*client, rest.data(), static_cast<int>(rest.size()));

    // Read real request
    if (!recv_exact(*client, hdr, 14)) {
      close(*client);
      return;
    }
    const std::uint32_t plen2 = read_u32_le(hdr + 10);
    const bool has_mac2 = (hdr[5] & static_cast<std::uint8_t>(C2Flags::HasMac)) != 0;
    std::vector<std::uint8_t> frame(hdr, hdr + 14);
    frame.resize(14 + plen2 + (has_mac2 ? 32u : 0u));
    if (plen2 + (has_mac2 ? 32u : 0u) > 0)
      recv_exact(*client, frame.data() + 14, static_cast<int>(plen2 + (has_mac2 ? 32u : 0u)));

    auto req = c2_unpack_frame(frame.data(), frame.size(), nullptr, key);
    if (!req) {
      close(*client);
      return;
    }

    C2Message ack;
    ack.type = C2MessageType::OffsetResponse;
    ack.seq = req->seq;
    ack.flags = static_cast<std::uint8_t>(C2Flags::Encrypted) |
                static_cast<std::uint8_t>(C2Flags::HasMac);
    const char* blob = "OFFSETS-OK";
    ack.payload.assign(blob, blob + std::strlen(blob));
    auto packed = c2_pack_frame(ack, key);
    if (packed)
      send_all(*client, packed->data(), static_cast<int>(packed->size()));
    close(*client);
    ok = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  C2Config cfg;
  cfg.endpoint = "127.0.0.1:" + std::to_string(port);
  cfg.auth_token = "c2-lab-token";
  cfg.transport = C2Config::Transport::TcpFramed;
  cfg.encrypt_payloads = true;
  cfg.connect_timeout_ms = 3000;
  cfg.io_timeout_ms = 3000;

  C2Client client(cfg);
  auto conn = client.connect();
  expect(static_cast<bool>(conn), "C2Client::connect");
  expect(client.is_connected(), "is_connected");
  expect(client.session_key().size() == 32, "session key derived");

  auto offsets = client.fetch_offsets("1.40.5");
  expect(static_cast<bool>(offsets), "fetch_offsets over TCP");
  if (offsets) {
    const std::string body(offsets->begin(), offsets->end());
    expect(body == "OFFSETS-OK", "offset payload from lab server");
  }

  client.disconnect();
  expect(!client.is_connected(), "disconnect clears connected");

  server.join();
  close(*listener);
  expect(ok.load() || static_cast<bool>(offsets), "c2 server handled request");
}

// ── Offset blob ────────────────────────────────────────────────────

static void test_offset_blob() {
  std::printf("\n=== offset blob ===\n");
  using namespace real::net;
  std::uint64_t offs[10] = {
      0x1000, 0x2000, 0x3000, 0x4000, 0x5000,
      0x6000, 0x7000, 0x8000, 0x9000, 0xA000};
  auto blob = build_offset_blob(0x14050001u, offs, 0x5A);
  expect(blob.size() > 10 && blob[0] == 0x5A, "build_offset_blob key prefix");

  // Parse locally without network: reimplement using shipped crypto helpers only
  // by exercising decrypt path identical to fetch_offsets body parse via a local HTTP server.
  const std::uint16_t port = 38779;
  auto listener = listen_tcp("127.0.0.1", port, 2);
  expect(static_cast<bool>(listener), "offset blob listen");
  if (!listener) return;

  std::thread server([&]() {
    auto c = accept(*listener, 5000);
    if (!c) return;
    std::uint8_t buf[2048];
    // drain request
    set_timeout(*c, 2000);
    recv(*c, buf, sizeof(buf));
    const std::string hdr =
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
        "Content-Length: " +
        std::to_string(blob.size()) + "\r\nConnection: close\r\n\r\n";
    send_all(*c, reinterpret_cast<const std::uint8_t*>(hdr.data()),
             static_cast<int>(hdr.size()));
    send_all(*c, blob.data(), static_cast<int>(blob.size()));
    close(*c);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/offsets.bin";
  auto od = fetch_offsets(url.c_str());
  expect(od.has_value(), "fetch_offsets from loopback");
  if (od) {
    expect(verify_offsets(*od, 0x14050001u), "verify_offsets version match");
    expect(od->dwEntityList == 0x1000, "dwEntityList from blob");
    expect(od->dwLocalPlayer == 0x2000, "dwLocalPlayer from blob");
    expect(od->dwViewMatrix == 0x3000, "dwViewMatrix from blob");
    expect(!verify_offsets(*od, 0xDEADBEEFu), "verify rejects wrong version");
  }
  server.join();
  close(*listener);
}

// ── Beacon ─────────────────────────────────────────────────────────

static void test_beacon() {
  std::printf("\n=== beacon scheduler ===\n");
  real::net::BeaconScheduler sched(50, 0);  // 50ms, no jitter
  expect(sched.ms_until_fire() >= 0, "ms_until_fire non-negative");
  // Wait until due
  for (int i = 0; i < 40 && !sched.should_fire(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  expect(sched.fire_count() >= 1, "beacon fired at least once");
}

// ── Named pipe framed (Windows) ────────────────────────────────────

static void test_named_pipe() {
  std::printf("\n=== named pipe ===\n");
#if LR_PLATFORM_WINDOWS
  using namespace real::net;
  const std::string name =
      "\\\\.\\pipe\\ac_lab_net_stack_" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());

  std::atomic<bool> server_ready{false};
  std::atomic<bool> ok{false};
  std::string server_err;
  std::thread server([&]() {
    NamedPipe srv;
    auto r = srv.create_server_nowait(name);
    if (!r) {
      server_err = r.error_msg.c_str();
      server_ready = true;
      return;
    }
    server_ready = true;
    if (!srv.wait_for_client(10000)) {
      server_err = "wait_for_client failed";
      return;
    }
    auto frame = srv.read_frame();
    if (!frame) {
      server_err = frame.error_msg.c_str();
      return;
    }
    const std::string echo(frame->begin(), frame->end());
    const std::string reply = "ECHO:" + echo;
    auto wr = srv.write_frame(reinterpret_cast<const std::uint8_t*>(reply.data()),
                               static_cast<int>(reply.size()));
    if (!wr) {
      server_err = wr.error_msg.c_str();
      return;
    }
    ok = true;
  });

  // Wait until server has created the pipe object
  for (int i = 0; i < 200 && !server_ready.load(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  NamedPipe cli;
  auto cr = cli.connect_client(name, 10000);
  expect(static_cast<bool>(cr), "pipe connect_client");
  if (!cr && !server_err.empty())
    std::fprintf(stderr, "  server_err: %s\n", server_err.c_str());
  if (cr) {
    const char* msg = "holder-reader-ui";
    auto w = cli.write_frame(reinterpret_cast<const std::uint8_t*>(msg),
                             static_cast<int>(std::strlen(msg)));
    expect(static_cast<bool>(w), "pipe write_frame");
    auto frame = cli.read_frame();
    expect(static_cast<bool>(frame), "pipe read_frame");
    if (frame) {
      const std::string got(frame->begin(), frame->end());
      expect(got == "ECHO:holder-reader-ui", "pipe frame echo");
    }
  }
  server.join();
  expect(ok.load(), "pipe server path");
#else
  expect(true, "named pipe skipped (non-Windows)");
#endif
}

// ── Structural: all domain sources present ─────────────────────────

static void test_structural_sources() {
  std::printf("\n=== structural sources ===\n");
  // Relative to typical build cwd; also try absolute from env.
  const char* files[] = {
      "lib/real/net/common.cpp",
      "lib/real/net/socket.cpp",
      "lib/real/net/http.cpp",
      "lib/real/net/c2_client.cpp",
      "lib/real/net/pipe.cpp",
      "lib/real/net/dns.cpp",
      "lib/real/net/offset_fetch.cpp",
      "lib/real/net/crypto.cpp",
      "lib/real/net/proxy.cpp",
      "lib/real/net/websocket.cpp",
      "lib/real/net/net_client.hpp",
      "lib/real/net/net_crypto.hpp",
      "lib/real/net/net_internal.hpp",
      "lib/real/net/offset_fetch.hpp",
  };
  int found = 0;
  for (const char* rel : files) {
    std::string paths[] = {
        std::string(rel),
        std::string("../") + rel,
        std::string("../../") + rel,
        std::string("code/") + rel,
        std::string("../code/") + rel,
    };
    bool ok = false;
    for (const auto& p : paths) {
      FILE* f = std::fopen(p.c_str(), "rb");
      if (f) {
        std::fclose(f);
        ok = true;
        break;
      }
    }
    if (ok) ++found;
    else std::fprintf(stderr, "  missing: %s\n", rel);
  }
  expect(found == static_cast<int>(sizeof(files) / sizeof(files[0])),
         "all net stack source files present");
}

int main() {
  std::printf("=== Net Stack Test ===\n");
  test_structural_sources();
  test_parse_url();
  test_crypto();
  test_dns_tunnel();
  test_c2_framing();
  test_beacon();
  test_loopback_http();
  test_c2_loopback();
  test_offset_blob();
  test_named_pipe();

  std::printf("\n=== Results: %d passed, %d failed ===\n", g_passes, g_fails);
  return g_fails == 0 ? 0 : 1;
}
