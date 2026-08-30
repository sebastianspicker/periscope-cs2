#include "red_example.hpp"
namespace examples::web_phone_radar {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("phone-bridge.exe");
  w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  w.add_net(sim::NetFlow{r.actor_pid, "radar-saas.example:443", false, true});
  // No overlay — phone is the UI.
  r.achieved = true;
  r.detail = "web_phone_radar red handle+saas no_overlay";
  w.note(r.detail);
  return r;
}
}  // namespace examples::web_phone_radar
