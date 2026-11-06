#include "blue_example.hpp"

#include <cstdio>

namespace examples::aim_humanization {

BlueResult detect(sim::World& w) {
  BlueResult r;

  // Independent signal 1: soft injected deltas (snap-only ML would miss these).
  bool soft = !w.inputs.empty();
  int injected = 0;
  for (const auto& e : w.inputs) {
    if (e.dx > 40.f || e.dy > 40.f) soft = false;
    if (e.source == "injected") ++injected;
  }
  if (injected > 0 && soft) {
    r.reasons.emplace_back("soft_injected_inputs count=" + std::to_string(injected));
  }

  // Independent signal 2: residual radar SaaS (intel product behind soft aim).
  bool saas = false;
  for (const auto& n : w.net) {
    if (n.looks_like_radar_saas) {
      saas = true;
      break;
    }
  }
  if (saas) r.reasons.emplace_back("radar_saas_channel residual");

  // Independent signal 3: multi-frame soft stream (not a single event).
  if (static_cast<int>(w.inputs.size()) >= 8 && soft) {
    r.reasons.emplace_back("multi_frame_soft_stream n=" +
                           std::to_string(w.inputs.size()));
  }

  const bool specific_scar = saas && !w.inputs.empty();
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: soft-aim + radar SaaS residual");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;
  // Soft slips pure snap detectors; residual multi-reason still holds for mitigate.
  r.mitigated = r.detected && soft;
  if (r.mitigated) w.ranked_access_denied = true;

  r.detail = "aim_humanization blue soft=" + std::to_string(soft ? 1 : 0) +
             " saas=" + std::to_string(saas ? 1 : 0) +
             " signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0);
  w.note(r.detail);
  std::printf("[aim_humanization] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[aim_humanization]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::aim_humanization
