#include "red_example.hpp"

#include <cstdio>

namespace examples::overwatch_multisignal {

RedResult apply(sim::World& w) {
  RedResult r;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    r.detail = "precondition failed: game process unavailable";
    return r;
  }
  ++steps;

  // Phase 1: weak technical scar — foreign VM_READ.
  r.actor_pid = w.spawn("multi-weak.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  if (!w.open_process(r.actor_pid, game_pid, sim::AccessMask::VmRead, false)) {
    r.detail = "OpenProcess VM_READ failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: network residual — VPN/proxy graph.
  w.vpn_proxy_active = true;
  ++steps;

  // Phase 3: polymorphic private build id (buyer uniqueness).
  w.binary_build_id = "poly-buyer-42";
  w.overwatch_score = 0;
  ++steps;

  bool has_handle = false;
  for (const auto& h : w.handles_to(game_pid)) {
    if (h.owner_pid == r.actor_pid && sim::has(h.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      break;
    }
  }
  r.achieved = has_handle && w.vpn_proxy_active && w.binary_build_id != "shared";
  r.steps = steps;
  r.detail = "overwatch_multisignal red handle+vpn+poly steps=" +
             std::to_string(steps);
  w.note(r.detail);
  std::printf("[overwatch_multisignal] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::overwatch_multisignal
