// radar_webradar_test.cpp — R5: web-radar service core with fake clients.
//
// Drives broadcast, interval gating, latest-frame coalescing, slow-client
// accounting, fallback payload, origin allowlist and stats without any
// socket (the transport adapter is lab-only; this is the acceptance the
// ledger asks for: "fake socket loop driving broadcast + slow-client drop
// counting").

#include "radar/webradar_service.hpp"

#include <cstdio>
#include <string>

#include "radar/snapshot.hpp"
#include "radar/intervals.hpp"

namespace {

using radar_application::WebRadarService;
using radar_domain::GameSnapshot;
using radar_domain::PlayerState;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

GameSnapshot snapshot_with_seq(std::uint64_t seq, const std::string& map) {
  GameSnapshot snap;
  snap.seq = seq;
  snap.t_capture_ms = 1000 + seq;
  snap.map = map;
  snap.local.player_id = 1;
  snap.local.team = radar_domain::kTeamCT;
  PlayerState player;
  player.player_id = 2;
  player.team = radar_domain::kTeamT;
  player.alive = true;
  player.pos = {1.0f, 2.0f, 3.0f};
  snap.players = {player};
  return snap;
}

WebRadarService::ServiceConfig enabled_config() {
  WebRadarService::ServiceConfig config;
  config.enabled = true;
  config.interval_ms = 100;
  return config;
}

void test_fallback_before_first_snapshot() {
  WebRadarService service;
  service.configure(enabled_config());
  service.accept_client(1, 1, "");
  const std::string payload = service.poll_client(1, 10);
  check(!payload.empty(), "fallback payload for fresh client");
  check(payload.find("\"fallback\":true") != std::string::npos,
        "fallback marked as such");
  check(payload.find("\"players\":[]") != std::string::npos,
        "fallback has empty players");
}

void test_broadcast_and_interval_gating() {
  WebRadarService service;
  service.configure(enabled_config());
  service.accept_client(1, 1, "");

  service.update_snapshot(snapshot_with_seq(1, "de_mirage"));
  const std::string first = service.poll_client(1, 100);
  check(!first.empty(), "first frame delivered");
  check(first.find("\"seq\":1") != std::string::npos, "frame content");
  check(first.find("de_mirage") != std::string::npos, "map in frame");

  // Same version polled again immediately: gated by interval.
  check(service.poll_client(1, 150).empty(), "interval gate blocks early");

  // New snapshot but polled before the interval: still gated.
  service.update_snapshot(snapshot_with_seq(2, "de_mirage"));
  check(service.poll_client(1, 160).empty(), "interval gate before new frame");

  // After the interval: the newest frame goes out.
  const std::string second = service.poll_client(1, 300);
  check(second.find("\"seq\":2") != std::string::npos,
        "latest frame after interval");
}

void test_slow_client_accounting() {
  WebRadarService service;
  service.configure(enabled_config());
  service.accept_client(1, 1, "");

  service.update_snapshot(snapshot_with_seq(1, "de_mirage"));
  check(!service.poll_client(1, 100).empty(), "initial send");

  // Five snapshots published while the client is silent.
  for (std::uint64_t seq = 2; seq <= 6; ++seq) {
    service.update_snapshot(snapshot_with_seq(seq, "de_mirage"));
  }
  // Client polls much later than twice the interval: slow client.
  const std::string latest = service.poll_client(1, 2000);
  check(latest.find("\"seq\":6") != std::string::npos,
        "slow client gets only the latest");
  check(service.stats().coalesced_frames == 4,
        "four superseded frames coalesced");
  check(service.stats().dropped_frames_slow_client == 4,
        "slow client drop counter incremented");
  check(service.stats().published_frames == 6, "publish count");
  check(service.stats().sent_frames == 2, "send count");
  check(service.stats().last_payload_bytes == latest.size(),
        "payload bytes recorded");
}

void test_backwards_clock_does_not_stall() {
  WebRadarService service;
  service.configure(enabled_config());
  service.accept_client(1, 1, "");

  service.update_snapshot(snapshot_with_seq(1, "de_mirage"));
  check(!service.poll_client(1, 100).empty(), "initial frame at t=100");

  // The clock steps backwards below last_sent_ms; the interval gate must
  // treat it as a full interval rather than blocking forever.
  service.update_snapshot(snapshot_with_seq(2, "de_mirage"));
  const std::string backwards = service.poll_client(1, 50);
  check(backwards.find("\"seq\":2") != std::string::npos,
        "backwards clock still delivers the newest frame");
}

void test_disabled_service_and_unknown_client() {
  WebRadarService service;
  WebRadarService::ServiceConfig config = enabled_config();
  config.enabled = false;
  service.configure(config);
  service.update_snapshot(snapshot_with_seq(1, "de_mirage"));
  check(service.poll_client(1, 100).empty(), "disabled service silent");
  check(!service.has_active_consumers(), "no consumers when disabled");

  service.configure(enabled_config());
  check(service.poll_client(999, 100).empty(),
        "unknown client gets nothing");
  service.accept_client(1, 1, "");
  check(service.has_active_consumers(), "consumer registered");
  check(!service.poll_client(1, 100).empty(),
        "enabled service delivers after accept");
}

void test_origin_allowlist() {
  WebRadarService service;
  WebRadarService::ServiceConfig config = enabled_config();
  config.origin_allowlist = {"http://192.168.1.50:8080"};
  service.configure(config);

  check(service.origin_allowed("http://192.168.1.50:8080"),
        "allowlisted origin allowed");
  check(!service.origin_allowed("http://evil.example.com"),
        "foreign origin rejected");

  service.accept_client(1, 1, "http://evil.example.com");
  check(!service.has_active_consumers(),
        "rejected origin never becomes a consumer");

  // Empty allowlist allows everything (LAN-bind decision gates exposure).
  WebRadarService::ServiceConfig open = enabled_config();
  service.configure(open);
  check(service.origin_allowed("http://anything"), "empty list permissive");
}

void test_protocol_version_and_stats_target() {
  WebRadarService service;
  WebRadarService::ServiceConfig config = enabled_config();
  service.configure(config);
  check(service.stats().target_hz == 10, "100 ms interval -> 10 Hz target");

  WebRadarService::ServiceConfig fast = enabled_config();
  fast.interval_ms = 5;  // below the floor -> clamped
  service.configure(fast);
  check(service.stats().target_hz ==
            1000 / radar_domain::kWebRadarMinIntervalMs,
        "interval clamped to domain floor");

  service.accept_client(1, 99, "origin");  // unknown protocol version
  // Version negotiation belongs to the transport; the core records it.
  check(service.has_active_consumers(), "client accepted regardless of v");
}

}  // namespace

int main() {
  test_fallback_before_first_snapshot();
  test_broadcast_and_interval_gating();
  test_slow_client_accounting();
  test_backwards_clock_does_not_stall();
  test_disabled_service_and_unknown_client();
  test_origin_allowlist();
  test_protocol_version_and_stats_target();

  if (failures != 0) {
    std::printf("%d webradar check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_webradar_test: all checks passed\n");
  return 0;
}
