#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::external_clone_display {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:external_clone_display] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:external_clone_display] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("external_clone_display-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:external_clone_display] step %d: apply isolated strategy residual\n", r.steps);
  w.external_display_clone = true; w.capture_vs_present_mismatch = true;
  if (!(w.external_display_clone)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:external_clone_display] step %d: validate recorded world state\n", r.steps);
  if (!w.capture_vs_present_mismatch) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "external_clone_display completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::external_clone_display
