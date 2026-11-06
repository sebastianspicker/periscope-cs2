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

}  // namespace t0_red
