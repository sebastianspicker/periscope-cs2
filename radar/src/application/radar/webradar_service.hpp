// webradar_service.hpp — R5: embedded web-radar service core (portable).
//
// Adapted from the research ledger (R5): cs2-dma `webradar.h` (Apache-2.0)
// supplies the service shape — interval clamps, per-client latest-frame
// backpressure, coalescing/drop counters, runtime stats, fallback payload,
// origin allowlist — and tiansongyu/cs2_cheat (MIT) supplies the endpoint
// layout and threat model (HTTP/WS token is not transport security; LAN
// only; never port-forward).
//
// This class is the transport-independent core: client events in, payload
// decisions out. The real socket loop (embedded HTTP server, WS handshake)
// is a lab-only adapter concern and deliberately not part of the domain of
// this file, so the backpressure and stats semantics are testable with fake
// clients (tests/radar_webradar_test.cpp).

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "radar/snapshot.hpp"

namespace radar_application {

class WebRadarService {
 public:
  struct ServiceConfig {
    bool enabled = false;
    std::uint32_t interval_ms = 100;  // clamped through domain intervals
    std::uint16_t listen_port = 22006;
    bool bind_lan = false;            // 127.0.0.1 when false
    std::vector<std::string> origin_allowlist;  // empty = allow all
  };

  struct RuntimeStats {
    bool enabled = false;
    std::uint64_t published_frames = 0;
    std::uint64_t sent_frames = 0;             // accepted by any client
    std::uint64_t coalesced_frames = 0;        // superseded before send
    std::uint64_t dropped_frames_slow_client = 0;
    std::uint64_t served_requests = 0;
    std::size_t last_payload_bytes = 0;
    std::uint64_t total_bytes_out = 0;
    std::uint32_t target_hz = 0;
    std::string active_map;
  };

  // A connected browser client. `protocol_version` is negotiated by the
  // transport; the core only understands version 1 (R1 protocol v1).
  void configure(const ServiceConfig& config);

  void accept_client(std::uint64_t client_id, int protocol_version,
                     const std::string& origin);
  void drop_client(std::uint64_t client_id);

  // Publishes a new absolute snapshot. Serialized once, version-stamped,
  // and immediately visible to every subsequent client poll.
  void update_snapshot(const radar_domain::GameSnapshot& snapshot);

  // Transport poll: returns the payload to send to this client now (empty
  // string = send nothing). Applies: publish interval clamp, per-client
  // latest-frame semantics, slow-client drop accounting, and the fallback
  // payload when no snapshot exists yet.
  std::string poll_client(std::uint64_t client_id, std::uint64_t now_ms);

  [[nodiscard]] RuntimeStats stats() const { return stats_; }
  [[nodiscard]] bool has_active_consumers() const { return !clients_.empty(); }
  [[nodiscard]] bool origin_allowed(const std::string& origin) const;

 private:
  struct ClientState {
    int protocol_version = 1;
    std::string origin;
    std::uint64_t last_sent_version = 0;
    std::uint64_t last_sent_ms = 0;
  };

  ServiceConfig config_{};
  RuntimeStats stats_{};

  radar_domain::GameSnapshot latest_;
  bool has_snapshot_ = false;
  std::uint64_t snapshot_version_ = 0;
  std::string latest_payload_;  // serialized once per snapshot

  std::map<std::uint64_t, ClientState> clients_;
};

// Single-cell latest-frame fallback for a fresh browser connection before
// the first snapshot arrives (cs2-dma's BuildFallbackLiveJson analogue):
// valid JSON, marks itself as a fallback, never blocks the page.
std::string build_fallback_live_json(const std::string& map_name,
                                     std::uint64_t now_ms);

}  // namespace radar_application
