#include "blue_example.hpp"

#include <algorithm>

namespace examples::heavens_gate_syscall {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game = w.game_pid();
  bool foreign_vm_read_handle = false;
  bool wow64_process_with_handle = false;
  bool raw_non_proxy_vm_read_handle = false;

  for (const auto& handle : w.handles_to(game, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) {
      continue;
    }

    const auto* owner = w.proc(handle.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_vm_read_handle = true;
    }
    if (owner != nullptr && owner->name == "wow64-stub.exe") {
      wow64_process_with_handle = true;
    }
    if (!handle.via_syscall_path && !handle.via_proxy) {
      raw_non_proxy_vm_read_handle = true;
    }
  }

  if (foreign_vm_read_handle) {
    result.reasons.emplace_back("foreign process holds a game VM_READ handle");
  }
  if (w.heavens_gate_transition) {
    result.reasons.emplace_back("WOW64 Heaven's Gate transition scar is present");
  }
  if (wow64_process_with_handle) {
    result.reasons.emplace_back("WOW64 stub process holds a game VM_READ handle");
  }
  if (raw_non_proxy_vm_read_handle) {
    result.reasons.emplace_back(
        "game VM_READ handle bypasses both syscall-path and proxy markers");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.risk = std::min(1.0, result.signals * 0.20);
  result.detected = foreign_vm_read_handle &&
                    (w.heavens_gate_transition || wow64_process_with_handle) &&
                    result.signals >= 2;
  result.mitigated = result.risk >= 0.6;
  result.detail = std::to_string(result.signals) + " signals risk=" +
                  std::to_string(result.risk);
  return result;
}

}  // namespace examples::heavens_gate_syscall
