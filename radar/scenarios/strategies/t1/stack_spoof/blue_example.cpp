#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::stack_spoof {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();
  int foreign_vm_read = 0;
  int syscall_path_handles = 0;

  std::printf("[blue:stack_spoof] sensor 1: foreign VM_READ handle graph\n");
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      ++foreign_vm_read;
    }
    if (h.via_syscall_path) ++syscall_path_handles;
  }
  const bool handle_hit = foreign_vm_read > 0;
  if (handle_hit) {
    r.reasons.emplace_back("foreign process holds game VM_READ handle");
  }
  if (syscall_path_handles > 0) {
    r.reasons.emplace_back("syscall-path handle observed on game target");
  }

  std::printf("[blue:stack_spoof] sensor 2: stack spoof origin scar\n");
  const bool spoof_scar = handle_hit && w.stack_spoof_on_read;
  if (w.stack_spoof_on_read) {
    r.reasons.emplace_back("read telemetry reports a spoofed stack origin");
  }
  if (spoof_scar) {
    r.reasons.emplace_back("handle hit combined with stack_spoof_on_read");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.22);
  r.detected = r.signals >= 2;
  r.mitigated =
      handle_hit && (w.stack_spoof_on_read || syscall_path_handles > 0);
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "signals=" + std::to_string(r.signals) +
             " handle_hit=" + (handle_hit ? "1" : "0") +
             " spoof=" + (w.stack_spoof_on_read ? "1" : "0");
  std::printf("[blue:stack_spoof] %s detected=%d mitigated=%d\n", r.detail.c_str(),
              r.detected, r.mitigated);
  return r;
}

}  // namespace examples::stack_spoof
