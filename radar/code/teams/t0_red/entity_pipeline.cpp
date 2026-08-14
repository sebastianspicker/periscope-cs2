// entity_pipeline.cpp — parse lab entity table via IMemoryBackend into EntitySnapshots.
// refresh() reads count + per-entity raw; living() filters alive (T0 radar path).

#include "t0_red/entity_pipeline.hpp"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <random>
#include <sstream>

namespace t0_red {

EntityPipeline::EntityPipeline(ac::IMemoryBackend& backend) : backend_(backend) {}

// EntityPipeline::refresh: Re-read entity table from backend at image_base; fill snapshots + stats.
ac::Status EntityPipeline::refresh(std::uint64_t image_base) {
  entities_.clear();
  ++stats_.refresh_count;
  stats_.last_alive = 0;
  stats_.last_enemies = 0;
  stats_.last_raw_count = 0;

  ac::ReadRequest count_req{image_base + 0x00 /* educational: entity-count field offset */,
                            sizeof(std::uint32_t)};
  auto count_res = backend_.read(count_req);
  if (count_res.status != ac::Status::Ok ||
      count_res.bytes.size() < sizeof(std::uint32_t)) {
    stats_.last_status = count_res.status;
    stats_.detail = "count_read_failed";
    return count_res.status;
  }
  std::uint32_t count = 0;
  std::memcpy(&count, count_res.bytes.data(), sizeof(count));
  stats_.last_raw_count = static_cast<int>(count);
  if (count > max_entities_) {
    stats_.last_status = ac::Status::InvalidArgument;
    stats_.detail = "count_too_large";
    return ac::Status::InvalidArgument;
  }

  struct EntRaw {
    float x, y, z;
    std::uint8_t team, alive, pad[2];
  };

  std::vector<std::uint32_t> read_order(count);
  std::iota(read_order.begin(), read_order.end(), 0);
  if (read_strategy_ != ReadStrategy::Sequential && count > 1) {
    // Deterministic per-refresh shuffling keeps this lab reproducible while
    // modelling non-sequential entity-table offsets.
    std::mt19937 rng(static_cast<std::uint32_t>(image_base) ^
                     static_cast<std::uint32_t>(stats_.refresh_count * 0x9e3779b9u));
    std::shuffle(read_order.begin(), read_order.end(), rng);
    if (std::is_sorted(read_order.begin(), read_order.end())) {
      std::reverse(read_order.begin(), read_order.end());
    }
  }

  for (const auto i : read_order) {
    // educational: entity table base at +0x10, each entry stride == sizeof(EntRaw) == 16
    const auto addr =
        image_base + 0x10 + static_cast<std::uint64_t>(i) * sizeof(EntRaw);
    auto er = backend_.read(ac::ReadRequest{addr, sizeof(EntRaw)});
    if (er.status != ac::Status::Ok || er.bytes.size() < sizeof(EntRaw)) {
      stats_.last_status = er.status;
      stats_.detail = "entity_read_failed_i=" + std::to_string(i);
      return er.status;
    }
    EntRaw raw{};
    std::memcpy(&raw, er.bytes.data(), sizeof(raw));
    ac::EntitySnapshot snap;
    snap.id = i;
    snap.origin = {raw.x, raw.y, raw.z};
    snap.team = raw.team;
    snap.alive = raw.alive != 0;
    if (snap.alive) {
      ++stats_.last_alive;
    }
    entities_.push_back(snap);
  }

  stats_.last_status = ac::Status::Ok;
  std::ostringstream oss;
  oss << "entities=" << entities_.size() << " alive=" << stats_.last_alive
      << " strategy=" << (read_strategy_ == ReadStrategy::Sequential
                                  ? "sequential"
                                  : read_strategy_ == ReadStrategy::Scattered
                                        ? "scattered"
                                        : "random_offset");
  stats_.detail = oss.str();
  return ac::Status::Ok;
}

// EntityPipeline::living: Return only alive EntitySnapshots from the last refresh.
std::vector<ac::EntitySnapshot> EntityPipeline::living() const {
  std::vector<ac::EntitySnapshot> out;
  for (const auto& e : entities_) {
    if (e.alive) {
      out.push_back(e);
    }
  }
  return out;
}

// EntityPipeline::enemies_of: Filter living entities not on the given team.
std::vector<ac::EntitySnapshot> EntityPipeline::enemies_of(
    std::uint8_t local_team) const {
  std::vector<ac::EntitySnapshot> out;
  for (const auto& e : entities_) {
    if (e.alive && e.team != local_team) {
      out.push_back(e);
    }
  }
  return out;
}

// ── Periscope-level methods ────────────────────────────────────

size_t EntityPipeline::collect_periscope(
    uint64_t entity_list_addr,
    bool (*read_fn)(uint64_t addr, void* buf, size_t size)) noexcept {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  entities_.clear();
  size_t count = periscope_collector_.collect(entity_list_addr, read_fn);

  // Convert PlayerData -> EntitySnapshot
  for (const auto& pd : periscope_collector_.players()) {
    if (!pd.valid) continue;
    ac::EntitySnapshot snap;
    snap.id = static_cast<uint32_t>(&pd - periscope_collector_.players().data());
    snap.origin = {pd.origin.x, pd.origin.y, pd.origin.z};
    snap.health = static_cast<int32_t>(pd.health);
    snap.team = static_cast<uint8_t>(pd.team);
    snap.alive = pd.alive;
    snap.dormant = pd.dormant;
    snap.is_local_player = (pd.pawnAddr == periscope_collector_.local().pawnAddr);
    entities_.push_back(snap);
  }

  stats_.last_raw_count = static_cast<int>(count);
  stats_.last_alive = 0;
  for (const auto& e : entities_) {
    if (e.alive) ++stats_.last_alive;
  }
  stats_.refresh_count++;
  stats_.last_status = ac::Status::Ok;

  // Update yaw state from local player
  if (periscope_collector_.local().valid) {
    yaw_state_.update(periscope_collector_.local().yaw, true);
  }

  return count;
#else
  (void)entity_list_addr;
  (void)read_fn;
  return 0;
#endif
}

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
void EntityPipeline::project_to_radar(
    const real::cs2::periscope::HudRadarSnapshot& hud,
    float yaw, float radar_to_texture_scale,
    const ac::Vec3& local_origin,
    std::vector<ac::EntitySnapshot>& out_entities) noexcept {
  out_entities.clear();
  for (const auto& e : entities_) {
    if (!e.alive) continue;

    real::cs2::periscope::Vector3 worldPos{e.origin.x, e.origin.y, e.origin.z};
    bool oob = false;
    auto screen = real::cs2::periscope::polar_map_player_to_radar(
        hud, worldPos, yaw, radar_to_texture_scale, &oob);

    // Apply origin hold for stability
    real::cs2::periscope::Vector3 current{e.origin.x, e.origin.y, e.origin.z};
    auto held = origin_hold_.hold(current, e.alive);

    ac::EntitySnapshot snap = e;
    // Update with projected coordinates
    snap.origin.x = screen.x;
    snap.origin.y = 0;
    snap.origin.z = screen.y;
    out_entities.push_back(snap);
  }
}
#endif

}  // namespace t0_red
