#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::etw_blind {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();
  int foreign_vm_read = 0;
  int syscall_path_handles = 0;

  std::printf("[blue:etw_blind] sensor 1: ETW pipeline integrity\n");
  const bool etw_broken = !w.etw_enabled || w.etw_ti_blind;
  if (!w.etw_enabled) {
    r.reasons.emplace_back("ETW provider disabled (pipeline broken)");
  }
  if (w.etw_ti_blind) {
    r.reasons.emplace_back("ETW Threat Intelligence telemetry is blind");
  }

  std::printf("[blue:etw_blind] sensor 2: handle graph (still works when ETW is dark)\n");
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

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.22);
  r.detected = r.signals >= 2;
  r.mitigated = etw_broken && handle_hit;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "signals=" + std::to_string(r.signals) + " risk=" +
             std::to_string(r.risk) + " etw_broken=" +
             (etw_broken ? "1" : "0") + " handle_hit=" + (handle_hit ? "1" : "0");
  std::printf("[blue:etw_blind] %s detected=%d mitigated=%d\n", r.detail.c_str(),
              r.detected, r.mitigated);
  return r;
}

}  // namespace examples::etw_blind
