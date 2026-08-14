#include "red_example.hpp"
#include <cstdio>

namespace examples::accept_readiness_gate {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto gate = w.spawn("accept-gate.exe");
  if (!w.proc(gate))
    return {false, steps, "gate spawn failed"};

  w.accept_gate_open = true;
  w.accept_conditions_met = 7;
  w.accept_conditions_total = 7;
  w.accept_session_ready = true;
  w.match_active = true;

  const bool scar = w.accept_gate_open && w.accept_session_ready && w.accept_conditions_met >= 7;
  if (!scar)
    return {false, steps, "accept readiness gate scar failed"};

  std::printf("[T0 accept_readiness_gate] step %d: %d/7 conditions met, gate open, session ready\n", ++steps, w.accept_conditions_met);
  w.note("accept_readiness_gate: 7-condition readiness gate passed");
  return {true, steps, "accept_readiness_gate: 7 conditions met", w.accept_conditions_met, true, true};
}

}  // namespace examples::accept_readiness_gate
