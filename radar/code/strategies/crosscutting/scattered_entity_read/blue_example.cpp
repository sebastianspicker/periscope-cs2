#include "blue_example.hpp"

#include <cstdio>

namespace examples::scattered_entity_read {

BlueResult detect(sim::World& w) {
  BlueResult result{};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    result.detail = "game unavailable";
    return result;
  }

  bool vm_read_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      vm_read_handle = true;
      break;
    }
  }
  if (w.scattered_read_pattern) {
    result.reasons.emplace_back("strategy scar: scattered entity read pattern");
    result.risk += 0.20;
  }
  if (w.scattered_read_count > 0) {
    result.reasons.emplace_back("scattered entity reads=" + std::to_string(w.scattered_read_count));
    result.risk += 0.10;
  }
  if (w.read_timing_jitter) {
    result.reasons.emplace_back("inter-read timing jitter observed");
    result.risk += 0.10;
  }
  if (vm_read_handle) {
    result.reasons.emplace_back("foreign VM_READ handle remains open");
    result.risk += 0.15;
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = (w.scattered_read_pattern || vm_read_handle) && result.signals >= 2;
  result.mitigated = result.risk >= 0.55;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "scattered pattern risk=" + std::to_string(result.risk);
  std::printf("[T0 scattered_entity_read] BLUE signals=%d risk=%.2f detected=%s\n",
              result.signals, result.risk, result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 scattered_entity_read]   %s\n", reason.c_str());
  }
  return result;
}

}  // namespace examples::scattered_entity_read
