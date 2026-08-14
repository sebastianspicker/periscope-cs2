#include "blue_example.hpp"

#include <algorithm>

namespace examples::enhanced_stack_spoof {

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
      result.reasons.push_back("foreign process holds a game VM_READ handle");
      break;
    }
  }

  if (w.stack_spoof_on_read) {
    result.reasons.push_back("read telemetry reports a spoofed stack origin");
  }
  if (w.enhanced_stack_spoof) {
    result.reasons.push_back("deep call-stack forgery is present");
  }
  if (w.spoofed_call_depth > 2) {
    result.reasons.push_back("spoofed call depth exceeds a shallow normal call chain");
  }
  if (w.enhanced_stack_spoof && w.spoofed_call_depth > 1) {
    result.reasons.push_back("forged trusted-module frames obscure the true read origin");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.risk = std::min(1.0, result.signals * 0.18);
  result.detected = foreign_vm_read_handle &&
                    (w.enhanced_stack_spoof || w.stack_spoof_on_read) &&
                    result.signals >= 2;
  // Attribution is less certain, so mitigation uses the dedicated lower threshold.
  result.mitigated = result.risk >= 0.55;
  result.detail = std::to_string(result.signals) + " signals risk=" +
                  std::to_string(result.risk) +
                  "; exact read origin attribution is degraded by the forged chain";
  return result;
}

}  // namespace examples::enhanced_stack_spoof
