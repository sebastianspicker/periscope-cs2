#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::cr3_stealth_target {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:cr3_stealth_target] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:cr3_stealth_target] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("cr3_stealth_target-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:cr3_stealth_target] step %d: apply isolated strategy residual\n", r.steps);
  if (auto* game = w.proc(w.game_pid())) game->cr3 = 0xC3A50000ull; w.trust.personal_hv_active = true;
  if (!(w.trust.personal_hv_active)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:cr3_stealth_target] step %d: validate recorded world state\n", r.steps);
  if (!(w.proc(w.game_pid()) != nullptr && w.proc(w.game_pid())->cr3 != 0)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "cr3_stealth_target completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "cr3_stealth_target", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::cr3_stealth_target
