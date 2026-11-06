#include "blue_example.hpp"

namespace examples::fallback_chain {

// BLUE: stack sensors — HV-only hunting misses RPM fallback.
BlueResult detect(sim::World& w) {
  BlueResult r;
  const auto game = w.game_pid();
  for (const auto& h : w.handles_to(game)) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) r.handle_hit = true;
    }
  }
  for (const auto& d : w.drivers) {
    if (!d.is_ac && d.provides_mem_rw) r.driver_hit = true;
  }
  r.detected = r.handle_hit && r.driver_hit;
  if (r.detected) {
    r.mitigated = true;
    w.byovd_policy_block = true;
    w.ranked_access_denied = true;
  }
  r.detail = "fallback_chain blue handle=" + std::to_string(r.handle_hit ? 1 : 0) +
             " driver=" + std::to_string(r.driver_hit ? 1 : 0);
  w.note(r.detail);
  return r;
}

}  // namespace examples::fallback_chain
