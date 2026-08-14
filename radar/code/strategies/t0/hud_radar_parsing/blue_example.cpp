#include "blue_example.hpp"
#include <cstdio>

namespace examples::hud_radar_parsing {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game) {
    result.reasons.emplace_back("game unavailable");
    result.signals = 1;
    return result;
  }

  int foreign_handles = 0;
  for (const auto& h : w.handles_to(game_pid)) {
    const auto* owner = w.proc(h.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac && sim::has(h.access, sim::AccessMask::VmRead))
      ++foreign_handles;
  }
  if (foreign_handles > 0) result.reasons.emplace_back("foreign VM_READ handles=" + std::to_string(foreign_handles));
  if (w.remote_read_ops > 0) result.reasons.emplace_back("remote read ops=" + std::to_string(w.remote_read_ops));
  if (w.hud_radar_snapshot_taken) {
    result.reasons.emplace_back("HudRadar snapshot taken");
    result.radar_snapshot_seen = true;
  }
  if (w.hud_radar_entity_count > 0) result.reasons.emplace_back("HudRadar entities=" + std::to_string(w.hud_radar_entity_count));
  if (w.hud_radar_bst_nodes_walked > 0) result.reasons.emplace_back("BST nodes walked=" + std::to_string(w.hud_radar_bst_nodes_walked));

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.radar_snapshot_seen;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 hud_radar_parsing] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::hud_radar_parsing
