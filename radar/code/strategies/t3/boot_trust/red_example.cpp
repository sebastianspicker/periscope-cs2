#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::boot_trust {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:boot_trust] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:boot_trust] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("boot_trust-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:boot_trust] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.secure_boot = false; w.trust.secure_launch = false;
  if (!(!w.trust.secure_boot)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:boot_trust] step %d: validate recorded world state\n", r.steps);
  if (!(!w.trust.secure_launch)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "boot_trust completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "boot_trust", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::boot_trust
