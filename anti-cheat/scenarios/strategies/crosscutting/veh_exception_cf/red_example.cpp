// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

namespace examples::veh_exception_cf {

// RED multi-step entry for `veh_exception_cf`.
// Plants handle + module + optional driver residuals on World.
RedResult apply(sim::World& w) {
  RedResult r;
  const auto actor = w.spawn("veh_exception_cf-actor.exe");
  r.actor_pid = actor;

  w.open_process(actor, w.game_pid(), sim::AccessMask::VmRead, false);
  if (auto* g = w.proc(w.game_pid())) {
    (void)w.read_mem(actor, w.game_pid(), g->base, 4, true);
    for (auto& m : g->modules) {
      if (m.name == "client.dll" || m.name == "game.exe") {
        m.text_hash = "patched";
        m.iat_hooked = true;
        break;
      }
    }
    g->has_foreign_thread = true;
  }
  w.load_driver(sim::Driver{"lab.sys", "labsha", "unknown", false, false, false, false, true});
  w.create_device(sim::Device{"\\\\.\\Lab", "lab.sys", true});

  r.achieved = true;
  r.detail = "veh_exception_cf red multi-step";
  w.note(r.detail);
  return r;
}

}  // namespace examples::veh_exception_cf
