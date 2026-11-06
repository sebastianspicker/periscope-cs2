#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::lag_switch {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:lag_switch] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:lag_switch] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("lag_switch-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:lag_switch] step %d: apply isolated strategy residual\n", r.steps);
  w.lag_switch_active = true; w.lag_switch_drop_bursts = 4; w.lag_switch_hold_ms = 180.f;
  if (!(w.lag_switch_active)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:lag_switch] step %d: validate recorded world state\n", r.steps);
  if (!(w.lag_switch_drop_bursts >= 2)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "lag_switch completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::lag_switch
