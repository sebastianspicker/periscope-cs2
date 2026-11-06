#pragma once

// Educational simulation: BehavioralDecoupler models observable patterns
// that anti-cheat systems detect. Part of a blue-team defensive simulator.
// Adds jitter/latency/omission to entity data for VACnet research.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace features {

struct DecouplerConfig {
  double frame_skip_rate = 0.02;
  double entity_skip_rate = 0.03;
  double position_noise_px = 2.0;
  int latency_ticks_min = 0;
  int latency_ticks_max = 3;
  double counter_strafe_jitter = 0.10;
  bool blind_spots_enabled = true;
  int blind_spot_min_ticks = 5;
  int blind_spot_max_ticks = 20;
};

struct BehavioralReport {
  std::uint64_t total_frames = 0;
  std::uint64_t frames_skipped = 0;
  std::uint64_t entities_omitted = 0;
  std::uint64_t entities_fuzzed = 0;
  double avg_latency_ticks = 0.0;
  double vacnet_risk_score = 0.0;
  std::string detail;
};

class BehavioralDecoupler {
 public:
  explicit BehavioralDecoupler(sim::World& world);
  void set_config(const DecouplerConfig& cfg);
  const DecouplerConfig& config() const { return config_; }

  ac::EntitySnapshot process(const ac::EntitySnapshot& raw,
                             std::uint64_t current_tick);
  std::vector<ac::EntitySnapshot> process_batch(
      const std::vector<ac::EntitySnapshot>& raw, std::uint64_t current_tick);
  int humanize_counter_strafe(int raw_delta);
  const BehavioralReport& report() const { return report_; }
  void reset();

 private:
  sim::World& world_;
  DecouplerConfig config_{};
  BehavioralReport report_{};
  std::mt19937_64 rng_{};

  struct BlindSpot {
    std::uint32_t entity_id;
    std::uint64_t expire_tick;
  };
  std::vector<BlindSpot> blind_spots_;

  double random_double();
  int random_int(int min, int max);
  bool should_skip_frame();
  bool should_skip_entity(std::uint32_t entity_id);
  ac::Vec3 fuzz_position(const ac::Vec3& pos);
  void update_vacnet_risk_score();
};

}  // namespace features
