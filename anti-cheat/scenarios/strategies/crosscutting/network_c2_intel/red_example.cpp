#include "red_example.hpp"
namespace examples::network_c2_intel {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("offset-loader.exe");
  w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  w.add_net(sim::NetFlow{r.actor_pid, "cdn.offsets.lab:443", true, false});
  w.add_net(sim::NetFlow{r.actor_pid, "auth.cheats.lab:443", true, false});
  r.intel_nets = 0;
  for (const auto& n : w.net) if (n.looks_like_offset_c2) ++r.intel_nets;
  r.achieved = r.intel_nets >= 1;
  r.detail = "network_c2_intel red intel_nets=" + std::to_string(r.intel_nets) + " + handle";
  w.note(r.detail);
  return r;
}
}  // namespace examples::network_c2_intel
