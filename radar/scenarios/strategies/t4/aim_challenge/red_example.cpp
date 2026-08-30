#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::aim_challenge {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:aim_challenge] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:aim_challenge] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("aim_challenge-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:aim_challenge] step %d: apply isolated strategy residual\n", r.steps);
  w.silent_aim_active = true; w.aim_samples.push_back(sim::AimSample{0.f, 0.f, 15.f, 0.f, false});
  if (!(w.silent_aim_active)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:aim_challenge] step %d: validate recorded world state\n", r.steps);
  if (!(!w.aim_samples.empty() && !w.aim_samples.back().challenge_passed)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "aim_challenge completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::aim_challenge
