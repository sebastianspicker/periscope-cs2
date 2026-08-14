#include "red_example.hpp"

#include <cstdio>

namespace examples::interest_mgmt {

// RED: memory radar product requires full enemy XY on client (structural want).
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

  // Phase 1: radar actor + VM_READ technical scar.
  const auto actor = w.spawn("interest-radar.exe");
  r.actor_pid = actor;
  if (!w.proc(actor)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    r.detail = "OpenProcess VM_READ failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: full-origin client product want (structural).
  w.server_sends_full_enemy_origin = true;
  w.client_entity_fidelity = 1.0f;
  w.client_replicated_enemy_budget = -1;
  r.wants_full_origin = true;
  ++steps;

  // Phase 3: stream key material on client (enables free XY even with crypto).
  w.client_has_stream_key = true;
  w.stream_key_exfiltrated = true;
  w.entity_stream_encrypted = false;
  r.key_exfil = true;
  ++steps;

  r.achieved = w.server_sends_full_enemy_origin && w.client_has_stream_key &&
               r.wants_full_origin && r.key_exfil;
  r.steps = steps;
  r.detail = "interest_mgmt red full_origin=1 client_key=1 vm_read=1 steps=" +
             std::to_string(steps);
  w.note(r.detail);
  std::printf("[interest_mgmt] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::interest_mgmt
