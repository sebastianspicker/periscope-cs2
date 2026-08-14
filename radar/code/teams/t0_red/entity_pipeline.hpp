#pragma once

// Parses lab/FPS synthetic entity layout via any IMemoryBackend.
// Layout (shared with sim::World::plant_lab_entities / fps lab_bridge):
//   base+0x00: u32 count
//   base+0x10: { f32 x,y,z, u8 team, u8 alive, pad[2] }...
//
// Enhanced with Periscope-level entity collection, coordinate math,
// multi-source yaw fusion and origin hold.

#include "ac/memory_backend.hpp"
#include "ac/types.hpp"

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
#include "real/cs2/periscope_entity.hpp"
#include "real/cs2/periscope_radar.hpp"
#include "real/cs2/periscope_hud.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace t0_red {

enum class ReadStrategy : std::uint8_t {
  Sequential,
  Scattered,
  RandomOffset,
};

// Lab type `EntityPipelineStats` used by this educational unit.
struct EntityPipelineStats {
  int refresh_count = 0;
  int last_raw_count = 0;
  int last_alive = 0;
  int last_enemies = 0;
  ac::Status last_status = ac::Status::Ok;
  std::string detail;
};

// Lab type `EntityPipeline` used by this educational unit.
class EntityPipeline {
 public:
  explicit EntityPipeline(ac::IMemoryBackend& backend);

  /// Read full entity table from image_base.
  ac::Status refresh(std::uint64_t image_base);

  /// Filter helpers for radar pedagogy.
  std::vector<ac::EntitySnapshot> living() const;
  std::vector<ac::EntitySnapshot> enemies_of(std::uint8_t local_team) const;

  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  const EntityPipelineStats& stats() const { return stats_; }

  void set_max_entities(std::uint32_t m) { max_entities_ = m; }
  void set_read_strategy(ReadStrategy strategy) { read_strategy_ = strategy; }
  ReadStrategy read_strategy() const { return read_strategy_; }

  // ── Periscope-level methods ────────────────────────────────────

  /// Collect entities using Periscope EntityCollector from a real CS2 process.
  /// Returns the number of valid players found.
  size_t collect_periscope(uint64_t entity_list_addr,
                            bool (*read_fn)(uint64_t addr, void* buf, size_t size)) noexcept;

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  /// Project collected entities into radar blips using Periscope coordinate math.
  void project_to_radar(
      const real::cs2::periscope::HudRadarSnapshot& hud,
      float yaw, float radar_to_texture_scale,
      const ac::Vec3& local_origin,
      std::vector<ac::EntitySnapshot>& out_entities) noexcept;

  /// Multi-source yaw fusion state (hold through bad frames).
  real::cs2::periscope::YawState& yaw_state() noexcept { return yaw_state_; }

  /// Origin hold state.
  real::cs2::periscope::OriginState& origin_hold() noexcept { return origin_hold_; }
#endif

 private:
  ac::IMemoryBackend& backend_;
  std::vector<ac::EntitySnapshot> entities_;
  EntityPipelineStats stats_{};
  std::uint32_t max_entities_ = 64;
  ReadStrategy read_strategy_ = ReadStrategy::Sequential;

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  real::cs2::periscope::EntityCollector periscope_collector_;
  real::cs2::periscope::YawState yaw_state_{};
  real::cs2::periscope::OriginState origin_hold_{};
#endif
};

}  // namespace t0_red
