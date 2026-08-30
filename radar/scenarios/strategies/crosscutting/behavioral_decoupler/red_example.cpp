#include "red_example.hpp"
#include "behavioral_decoupler.hpp"
#include <string>
#include <vector>

namespace examples::behavioral_decoupler {

void Red::apply(sim::World& w) noexcept {
  // Phase 1: configure multi-surface decoupler (skip / omit / fuzz / latency).
  features::BehavioralDecoupler decoupler(w);
  features::DecouplerConfig config;

  config.frame_skip_rate = 0.02;
  config.entity_skip_rate = 0.03;
  config.position_noise_px = 2.0;
  config.latency_ticks_min = 0;
  config.latency_ticks_max = 3;
  config.counter_strafe_jitter = 0.10;
  config.blind_spots_enabled = true;
  config.blind_spot_min_ticks = 5;
  config.blind_spot_max_ticks = 20;

  decoupler.set_config(config);

  // Phase 2: process entity batch (plants skip/omit/fuzz scars via library).
  std::vector<ac::EntitySnapshot> entities;
  entities.reserve(100);
  for (int i = 0; i < 100; ++i) {
    entities.emplace_back(static_cast<std::uint32_t>(i),
                          ac::Vec3{100.0f + static_cast<float>(i),
                                   200.0f, 50.0f},
                          2, true);
  }
  decoupler.process_batch(entities, 0);

  // Phase 3: humanize counter-strafe inputs (second scar class).
  int strafe_result = decoupler.humanize_counter_strafe(5);
  strafe_result += decoupler.humanize_counter_strafe(12);
  (void)strafe_result;

  // Phase 4: optional co-residual actor for blue handle correlation.
  const auto actor = w.spawn("behavioral-decoupler.exe");
  (void)w.open_process(actor, w.game_pid(), sim::AccessMask::VmRead, false);

  const auto& report = decoupler.report();
  w.note("behavioral_decoupler: frames_skipped=" +
         std::to_string(report.frames_skipped) +
         " omitted=" + std::to_string(report.entities_omitted) +
         " fuzzed=" + std::to_string(report.entities_fuzzed) +
         " latency=" + std::to_string(report.avg_latency_ticks) +
         " risk=" + std::to_string(report.vacnet_risk_score));
}

} // namespace
