#include "blue_example.hpp"
namespace examples::web_phone_radar {
BlueResult detect(sim::World& w) {
  BlueResult r;
  bool handle = false, saas = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) handle = true;
    }
  }
  for (const auto& n : w.net) if (n.looks_like_radar_saas) saas = true;
  r.detected = handle && saas;
  if (r.detected) r.mitigated = true;
  r.detail = "web_phone_radar blue handle=" + std::to_string(handle?1:0) + " saas=" + std::to_string(saas?1:0) +
             " overlays=" + std::to_string(w.overlays.size());
  w.note(r.detail);
  return r;
}
}  // namespace examples::web_phone_radar
