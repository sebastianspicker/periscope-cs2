#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::clipcursor {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:clipcursor] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:clipcursor] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("clipcursor-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:clipcursor] step %d: apply isolated strategy residual\n", r.steps);
  w.clipcursor_confined = true; w.push_input(sim::InputEvent{1.0, "injected", 5.f, 0.f});
  if (!(w.clipcursor_confined)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:clipcursor] step %d: validate recorded world state\n", r.steps);
  if (!(!w.inputs.empty() && w.inputs.back().source == "injected")) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "clipcursor completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::clipcursor
