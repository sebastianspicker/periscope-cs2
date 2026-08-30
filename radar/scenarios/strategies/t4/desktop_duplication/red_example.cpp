#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::desktop_duplication {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:desktop_duplication] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:desktop_duplication] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("desktop_duplication-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:desktop_duplication] step %d: apply isolated strategy residual\n", r.steps);
  w.desktop_duplication = true; w.capture_sensor_active = true;
  if (!(w.desktop_duplication)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:desktop_duplication] step %d: validate recorded world state\n", r.steps);
  if (!(w.capture_sensor_active)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "desktop_duplication completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::desktop_duplication
