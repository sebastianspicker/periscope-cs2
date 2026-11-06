#include "blue_example.hpp"

#include <algorithm>

namespace examples::dynamic_ssn_resolve {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game = w.game_pid();
  bool foreign_vm_read_handle = false;
  bool syscall_vm_read_handle = false;

  for (const auto& handle : w.handles_to(game, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) {
      continue;
    }

    const auto* owner = w.proc(handle.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_vm_read_handle = true;
      syscall_vm_read_handle = syscall_vm_read_handle || handle.via_syscall_path;
    }
  }

  if (w.dynamic_ssn_resolved) {
    result.reasons.emplace_back("ntdll export stubs were parsed for dynamic syscall numbers");
  }
  if (foreign_vm_read_handle) {
    result.reasons.emplace_back("foreign process holds a game VM_READ handle");
  }
  if (syscall_vm_read_handle) {
    result.reasons.emplace_back("foreign game VM_READ handle used the syscall path");
  }
  if (w.resolved_ssn_windows_build != 0) {
    result.reasons.emplace_back("runtime-specific Windows build SSN resolution is present");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.risk = std::min(1.0, result.signals * 0.20);
  result.detected = result.signals >= 2;
  // Build-correct SSNs defeat static tables, but operation-to-SSN tracking remains viable.
  result.mitigated = result.risk >= 0.60;
  result.detail = std::to_string(result.signals) + " signals risk=" +
                  std::to_string(result.risk) +
                  "; SSN-to-operation fingerprinting remains possible";
  return result;
}

}  // namespace examples::dynamic_ssn_resolve
