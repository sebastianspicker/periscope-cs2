#include "blue_example.hpp"

#include <algorithm>

namespace examples::ntdll_hook_evade {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game = w.game_pid();
  bool foreign_vm_read_handle = false;

  for (const auto& handle : w.handles_to(game, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) {
      continue;
    }

    const auto* owner = w.proc(handle.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_vm_read_handle = true;
      break;
    }
  }

  if (w.ntdll_hooks_detected) {
    result.reasons.emplace_back("process compared ntdll.text against a clean disk image");
  }
  if (w.used_clean_ntdll_copy) {
    result.reasons.emplace_back("clean ntdll copy was mapped to evade user-mode hooks");
  }
  if (w.hooks_evaded_count > 0) {
    result.reasons.emplace_back("anti-cheat ntdll hooks were identified and evaded");
  }
  if (foreign_vm_read_handle) {
    result.reasons.emplace_back("foreign process still holds a game VM_READ handle");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.risk = std::min(1.0, result.signals * 0.20);
  result.detected = result.signals >= 2;
  // Direct syscalls or a clean mapping blind API hooks, but cannot erase the handle scar.
  result.mitigated = result.risk >= 0.55;
  result.detail = std::to_string(result.signals) + " signals risk=" +
                  std::to_string(result.risk) +
                  "; hook telemetry is lost, while the handle graph remains visible";
  return result;
}

}  // namespace examples::ntdll_hook_evade
