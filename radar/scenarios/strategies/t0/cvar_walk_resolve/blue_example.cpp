#include "blue_example.hpp"
#include <cstdio>

namespace examples::cvar_walk_resolve {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.cvar_walk_resolved) {
    result.reasons.emplace_back("CVar walk resolved");
    result.resolve_detected = true;
  }
  if (w.cvar_walk_tier_reached > 0) result.reasons.emplace_back("CVar tier reached=" + std::to_string(w.cvar_walk_tier_reached));
  if (w.cvar_walk_entries_found > 0) result.reasons.emplace_back("CVar entries found=" + std::to_string(w.cvar_walk_entries_found));
  if (w.remote_read_ops > 0) result.reasons.emplace_back("remote read ops=" + std::to_string(w.remote_read_ops));
  for (const auto& h : w.handles_to(w.game_pid())) {
    const auto* owner = w.proc(h.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac && sim::has(h.access, sim::AccessMask::VmRead)) {
      result.reasons.emplace_back("foreign VM_READ handle");
      break;
    }
  }
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.resolve_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 cvar_walk_resolve] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::cvar_walk_resolve
