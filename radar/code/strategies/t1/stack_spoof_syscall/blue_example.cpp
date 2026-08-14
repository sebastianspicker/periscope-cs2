#include "blue_example.hpp"
#include <cstdio>

namespace examples::stack_spoof_syscall {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.stack_spoof_syscall_active) {
    result.reasons.emplace_back("stack spoof syscall active");
    result.stack_spoof_detected = true;
  }
  if (w.stack_spoof_ret_addr_forged) result.reasons.emplace_back("return address forged");
  if (w.stack_spoof_call_depth > 0) result.reasons.emplace_back("spoof call depth=" + std::to_string(w.stack_spoof_call_depth));
  if (w.stack_spoof_on_read) result.reasons.emplace_back("stack spoof on read active");
  if (w.enhanced_stack_spoof) result.reasons.emplace_back("enhanced stack spoof active, depth=" + std::to_string(w.spoofed_call_depth));

  for (const auto& h : w.handles_to(w.game_pid())) {
    if (h.via_syscall_path) {
      result.reasons.emplace_back("syscall path handle detected");
      break;
    }
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.stack_spoof_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 stack_spoof_syscall] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::stack_spoof_syscall
