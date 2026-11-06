#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::ept_hide_ac {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:ept_hide_ac] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:ept_hide_ac] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("ept_hide_ac-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:ept_hide_ac] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.personal_hv_active = true; w.trust.ept_hide_ac_pages = true; w.trust.guest_ac_view_clean = true;
  if (!(w.trust.ept_hide_ac_pages)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:ept_hide_ac] step %d: validate recorded world state\n", r.steps);
  if (!(w.trust.personal_hv_active)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "ept_hide_ac completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "ept_hide_ac", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::ept_hide_ac
