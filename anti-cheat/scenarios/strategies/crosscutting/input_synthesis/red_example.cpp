#include "red_example.hpp"

#include <cstdio>

namespace examples::input_synthesis {

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

  // Phase 1: MCU bridge actor (memory-clean path — no game handle required).
  r.actor_pid = w.spawn("kmbox-bridge.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: multi-source HID synthesis stream (arduino + kmbox).
  w.push_input(sim::InputEvent{0.1, "serial_arduino", 10.f, 4.f});
  w.push_input(sim::InputEvent{0.2, "kmbox", 9.f, 3.f});
  w.push_input(sim::InputEvent{0.3, "serial_arduino", 11.f, 2.f});
  if (w.inputs.size() < 3) {
    r.detail = "multi MCU input plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 3: raw_sendinput mix residual (improper HID+injected mix).
  w.raw_sendinput_mixed = true;
  if (!w.raw_sendinput_mixed) {
    r.detail = "raw_sendinput_mixed plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  r.achieved = w.inputs.size() >= 3 && w.raw_sendinput_mixed;
  r.steps = steps;
  r.detail = "input_synthesis red multi MCU sources n=" +
             std::to_string(w.inputs.size()) +
             " raw_mixed=1 steps=" + std::to_string(steps);
  w.note(r.detail);
  std::printf("[input_synthesis] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::input_synthesis
