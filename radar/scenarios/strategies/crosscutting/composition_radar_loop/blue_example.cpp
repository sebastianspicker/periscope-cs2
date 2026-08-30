#include "blue_example.hpp"
#include "strategies/crosscutting/cs_round_radar/blue_example.hpp"
#include "strategies/crosscutting/interest_mgmt/blue_example.hpp"
#include "strategies/t0/pattern_offset_scan/blue_example.hpp"

namespace examples::composition_radar_loop {

// Multi-sensor: pattern residuals + CS fog/fidelity (scenario-aware when possible).
BlueResult detect(sim::World& w) {
  BlueResult r;
  auto pat = examples::pattern_offset_scan::detect(w);
  auto cs = examples::cs_round_radar::detect(w);
  auto fog = examples::interest_mgmt::detect(w);

  r.pattern_hit = pat.detected;
  r.cs_hit = cs.detected;
  r.fog_hit = fog.detected || fog.mitigated || !w.server_sends_full_enemy_origin;

  if (r.pattern_hit) r.reasons.push_back("pattern_offset_scan");
  if (r.cs_hit) r.reasons.push_back("cs_round_radar");
  if (r.fog_hit) r.reasons.push_back("interest_management");

  // Multi-reason: require >=2 independent stack sensors (not single-sensor OR).
  r.detected = static_cast<int>(r.reasons.size()) >= 2;
  r.fog = r.fog_hit;
  r.mitigated = r.fog && (r.pattern_hit || r.cs_hit);
  if (r.mitigated) r.reasons.push_back("structural_mitigation");

  std::string note = "composition blue pat=" + std::to_string(r.pattern_hit) +
                     " cs=" + std::to_string(r.cs_hit) +
                     " fog=" + std::to_string(r.fog_hit) +
                     " mitig=" + std::to_string(r.mitigated) +
                     " reasons=" + std::to_string(r.reasons.size());
  w.note(note);
  return r;
}

}  // namespace examples::composition_radar_loop
