// webradar_service.cpp — R5 web-radar service core. See webradar_service.hpp.

#include "radar/webradar_service.hpp"

#include <algorithm>

#include "radar/intervals.hpp"
#include "radar/snapshot.hpp"

namespace radar_application {

std::string build_fallback_live_json(const std::string& map_name,
                                     std::uint64_t now_ms) {
  // Mirrors the snapshot layout so a fresh page can render immediately and
  // cleanly switch to real frames (cs2-dma fallback-payload behaviour).
  std::string out = "{\"v\":1,\"fallback\":true,\"seq\":0,\"t_capture_ms\":";
  out += std::to_string(now_ms);
  out += ",\"map\":\"";
  out += radar_domain::json_escape(map_name);
  out += "\",\"local\":{\"id\":\"0\",\"team\":0,\"name\":\"\",\"alive\":false,"
         "\"pos\":[0,0,0],\"yaw\":0,\"health\":0,\"armor\":0,\"money\":0,"
         "\"defuser\":false,\"bomb\":false,\"weapon\":0,\"ammo\":[0,0]},"
         "\"players\":[],\"entities\":[],\"bomb\":{\"planted\":false,"
         "\"ticking\":false,\"being_defused\":false,\"dropped\":false,"
         "\"pos\":[0,0,0],\"blow_time\":0,\"defuse_end_time\":0,"
         "\"timer_length\":0}}";
  return out;
}

void WebRadarService::configure(const ServiceConfig& config) {
  config_ = config;
  config_.interval_ms = radar_domain::clamped_interval_ms(config.interval_ms);
  stats_.enabled = config_.enabled;
  stats_.target_hz =
      config_.enabled
          ? 1000u / std::max<std::uint32_t>(config_.interval_ms, 1u)
          : 0u;
}

bool WebRadarService::origin_allowed(const std::string& origin) const {
  if (config_.origin_allowlist.empty()) {
    return true;  // unfiltered: the LAN-bind decision gates exposure
  }
  for (const std::string& allowed : config_.origin_allowlist) {
    if (origin == allowed) {
      return true;
    }
  }
  return false;
}

void WebRadarService::accept_client(std::uint64_t client_id,
                                    int protocol_version,
                                    const std::string& origin) {
  if (!config_.enabled) {
    return;
  }
  if (!origin_allowed(origin)) {
    return;
  }
  ClientState state;
  state.protocol_version = protocol_version;
  state.origin = origin;
  clients_[client_id] = state;
  ++stats_.served_requests;
}

void WebRadarService::drop_client(std::uint64_t client_id) {
  clients_.erase(client_id);
}

void WebRadarService::update_snapshot(
    const radar_domain::GameSnapshot& snapshot) {
  if (!config_.enabled) {
    return;
  }
  latest_ = snapshot;
  has_snapshot_ = true;
  latest_payload_ = radar_domain::serialize_snapshot_v1(snapshot);
  ++snapshot_version_;
  ++stats_.published_frames;
  stats_.last_payload_bytes = latest_payload_.size();
  stats_.active_map = snapshot.map;
}

std::string WebRadarService::poll_client(std::uint64_t client_id,
                                         std::uint64_t now_ms) {
  const auto it = clients_.find(client_id);
  if (it == clients_.end() || !config_.enabled) {
    return {};
  }
  ClientState& client = it->second;

  // Fresh browser before the first snapshot: immediate fallback payload.
  if (!has_snapshot_) {
    ++stats_.served_requests;
    return build_fallback_live_json("default", now_ms);
  }

  // Publish interval gate.
  // A clock that went backwards must not stall the client behind the
  // interval gate: treat the step as "a full interval has passed".
  const bool clock_went_backwards = now_ms < client.last_sent_ms;
  const std::uint64_t elapsed =
      clock_went_backwards ? config_.interval_ms : now_ms - client.last_sent_ms;
  if (client.last_sent_ms != 0 && elapsed < config_.interval_ms) {
    return {};
  }

  // Nothing new since the client's last send.
  if (client.last_sent_version == snapshot_version_) {
    return {};
  }

  // Latest-frame semantics: frames the client never observed are counted as
  // coalesced (superseded before send). When the client's own poll cadence
  // is slow (more than twice the publish interval between polls) the same
  // loss is additionally attributed to client slowness — the pair of
  // counters distinguishes producer coalescing from consumer backpressure.
  if (client.last_sent_version != 0) {
    const std::uint64_t missed =
        snapshot_version_ - client.last_sent_version - 1;
    if (missed > 0) {
      stats_.coalesced_frames += missed;
      if (elapsed > 2 * static_cast<std::uint64_t>(config_.interval_ms)) {
        stats_.dropped_frames_slow_client += missed;
      }
    }
  }

  client.last_sent_version = snapshot_version_;
  client.last_sent_ms = now_ms;
  ++stats_.sent_frames;
  stats_.total_bytes_out += latest_payload_.size();
  return latest_payload_;
}

}  // namespace radar_application
