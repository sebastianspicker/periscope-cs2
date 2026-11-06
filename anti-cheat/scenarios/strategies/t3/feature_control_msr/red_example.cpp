#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::feature_control_msr {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:feature_control_msr] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:feature_control_msr] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("feature_control_msr-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:feature_control_msr] step %d: apply isolated strategy residual\n", r.steps);
  w.feature_control_spoofed = true; w.trust.personal_hv_active = true;
  if (!(w.feature_control_spoofed)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:feature_control_msr] step %d: validate recorded world state\n", r.steps);
  if (!(w.trust.personal_hv_active)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "feature_control_msr completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "feature_control_msr", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::feature_control_msr
