#include "blue_example.hpp"
namespace examples::network_c2_intel {
BlueResult detect(sim::World& w) {
  BlueResult r;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) r.handle_hit = true;
    }
  }
  for (const auto& n : w.net) if (n.looks_like_offset_c2) r.intel_hit = true;
  // Never C2 alone — pair with client scar.
  r.detected = r.handle_hit && r.intel_hit;
  if (r.detected) { r.mitigated = true; w.ranked_access_denied = true; }
  r.detail = "network_c2_intel blue handle=" + std::to_string(r.handle_hit?1:0) +
             " intel=" + std::to_string(r.intel_hit?1:0);
  w.note(r.detail);
  return r;
}
}  // namespace examples::network_c2_intel
