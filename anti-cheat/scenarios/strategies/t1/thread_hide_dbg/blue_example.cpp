// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::thread_hide_dbg {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();

  const bool thread_hide_hit = w.thread_hide_from_debugger;
  const bool peb_spoof_hit = w.peb_being_debugged_spoofed;

  int foreign_vm_read = 0;
  for (const auto& handle : w.handles_to(game, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* owner = w.proc(handle.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      ++foreign_vm_read;
    }
  }
  const bool handle_hit = foreign_vm_read > 0;

  if (thread_hide_hit) {
    r.reasons.emplace_back("ThreadHideFromDebugger class scar is present");
  }
  if (peb_spoof_hit) {
    r.reasons.emplace_back("PEB BeingDebugged / anti-debug spoof is present");
  }
  if (handle_hit) {
    r.reasons.emplace_back("foreign process holds a game VM_READ handle");
  }
  if (thread_hide_hit && peb_spoof_hit) {
    r.reasons.emplace_back("thread hide and PEB spoof co-occur (anti-debug suite)");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.20);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  r.detail = std::to_string(r.signals) + " signals risk=" + std::to_string(r.risk) +
             " thread_hide=" + std::to_string(thread_hide_hit ? 1 : 0) +
             " peb_spoof=" + std::to_string(peb_spoof_hit ? 1 : 0) +
             " handle=" + std::to_string(handle_hit ? 1 : 0);
  std::printf("[blue:thread_hide_dbg] %s\n", r.detail.c_str());
  return r;
}

}  // namespace examples::thread_hide_dbg
