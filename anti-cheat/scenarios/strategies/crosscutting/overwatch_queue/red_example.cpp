#include "red_example.hpp"

#include <cstdio>

namespace examples::overwatch_queue {

// RED: mild handle + second weak residual (unique build) — no rage, no invent-in-blue.
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

  // Phase 1: mild foreign handle scar.
  r.actor_pid = w.spawn("mild-radar.exe");
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

  // Phase 2: second weak scar planted by red (private build), not by blue detect.
  w.binary_build_id = "mild-private";
  ++steps;

  bool has_handle = false;
  for (const auto& h : w.handles_to(game_pid)) {
    if (h.owner_pid == r.actor_pid && sim::has(h.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      break;
    }
  }
  r.achieved = has_handle && w.binary_build_id != "shared";
  r.steps = steps;
  r.detail = "overwatch_queue red mild handle + unique_build=" + w.binary_build_id +
             " steps=" + std::to_string(steps);
  w.note(r.detail);
  std::printf("[overwatch_queue] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::overwatch_queue
