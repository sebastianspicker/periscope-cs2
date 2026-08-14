#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::capture_cv_hid {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:capture_cv_hid] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:capture_cv_hid] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("capture_cv_hid-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:capture_cv_hid] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.capture_card_present = true; w.push_input(sim::InputEvent{1.0, "serial_arduino", 8.f, 2.f});
  if (!(w.trust.capture_card_present)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:capture_cv_hid] step %d: validate recorded world state\n", r.steps);
  if (!(!w.inputs.empty() && w.inputs.back().source == "serial_arduino")) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "capture_cv_hid completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::capture_cv_hid
