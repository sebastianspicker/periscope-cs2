#pragma once

// Educational simulation: EvasionMaster models observable anti-cheat signals.
// This is part of a blue-team defensive anti-cheat simulator.
// It sets simulation flags on World processes so the blue team can
// detect and learn about cheat behaviors. No real evasion occurs.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace sim {

struct EvasionConfig {
  double jitter_variance = 0.02;
  double frame_drop_rate = 0.02;
  double entity_omission_rate = 0.03;
  double position_fuzz_px = 3.0;
  double latency_ms_min = 0.0;
  double latency_ms_max = 5.0;
  bool disguise_enabled = true;
  ac::DisguiseProfile active_disguise = ac::DisguiseProfile::RivaTuner;
  bool read_order_shuffle = true;
  bool pattern_timing_jitter = true;
  int proxy_rotation_interval = 300;
  double decoy_read_ratio = 0.40;
  bool forensic_clean_enabled = true;
  bool peb_unlink_enabled = true;
  bool working_set_trim_enabled = true;
  bool thread_hide_enabled = true;
  bool temporal_phase_enabled = true;
  int temporal_phase_count = 4;
};

struct EvasionState {
  std::uint64_t tick_count = 0;
  std::uint64_t frames_rendered = 0;
  std::uint64_t frames_dropped = 0;
  std::uint64_t entities_omitted = 0;
  std::uint64_t decoy_reads = 0;
  double total_fuzz_applied_px = 0.0;
  double total_latency_ms = 0.0;
  int temporal_phase = 0;
  int phase_transitions = 0;
  bool forensic_cleaned = false;
  bool disguise_applied = false;
  bool peb_unlinked = false;
  bool working_set_trimmed = false;
  std::vector<ac::DefenseLayer> active_layers;
};

class EvasionMaster {
 public:
  explicit EvasionMaster(World& world);

  void init(const EvasionConfig& config, std::uint32_t cheat_pid);

  std::vector<ac::EntitySnapshot> tick(
      std::vector<ac::EntitySnapshot> entities,
      std::uint64_t current_tick,
      std::uint32_t cheat_pid);

  void shutdown(std::uint32_t cheat_pid);

  /// Apply PEB / working-set / thread-hide scars for |cheat_pid|.
  void apply_memory_hide(std::uint32_t cheat_pid);

  /// Forensic cleanup scars (prefetch / recent / PEB restore story).
  void apply_forensic_clean(std::uint32_t cheat_pid);

  /// Sample a latency delay in [min,max] and accumulate state.
  double sample_latency_ms();

  /// Whether this tick should issue a decoy (non-game) read residual.
  bool should_decoy_read();

  const EvasionState& state() const { return state_; }
  const EvasionConfig& config() const { return config_; }

 private:
  World& world_;
  EvasionConfig config_{};
  EvasionState state_{};
  std::mt19937_64 rng_{};
  std::uint32_t cheat_pid_ = 0;

  bool should_drop_frame();
  bool should_omit_entity(std::uint32_t entity_id);
  ac::Vec3 fuzz_position(const ac::Vec3& pos);
  std::vector<ac::EntitySnapshot> shuffle_entities(
      std::vector<ac::EntitySnapshot> entities);
  void apply_disguise(std::uint32_t cheat_pid);
  void populate_defense_layers();
  void advance_temporal_phase();
  double random_double();
};

}  // namespace sim
