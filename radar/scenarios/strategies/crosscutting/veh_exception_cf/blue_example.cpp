// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

namespace examples::veh_exception_cf {

// BLUE multi-reason entry for `veh_exception_cf`.
// Compose handle graph, module integrity, and platform/driver residuals.
BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  const auto game = w.game_pid();
  for (const auto& h : w.handles_to(game)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) ++reasons;
  }
  if (const auto* g = w.proc(game)) {
    for (const auto& m : g->modules) {
      if (m.text_hash != "clean" || m.iat_hooked || m.eat_hooked || m.present_hooked) ++reasons;
    }
    if (g->has_foreign_thread || g->thread_hijacked || g->hollowed) ++reasons;
  }
  if (w.trust.personal_hv_active || w.trust.dma_device_present || !w.trust.secure_boot) ++reasons;
  for (const auto& d : w.drivers) {
    if (!d.is_ac && d.provides_mem_rw) { ++reasons; break; }
  }

  r.detected = reasons >= 2;
  r.mitigated = reasons >= 2;
  if (r.mitigated) {
    w.ranked_access_denied = true;
  }
  r.detail = "veh_exception_cf blue reasons=" + std::to_string(reasons);
  return r;
}

}  // namespace examples::veh_exception_cf
