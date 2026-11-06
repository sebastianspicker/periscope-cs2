#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::secure_kernel_view {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:secure_kernel_view] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:secure_kernel_view] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("secure_kernel_view-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:secure_kernel_view] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.secure_kernel_view_dirty = true; w.trust.guest_ac_view_clean = true;
  if (!(w.trust.secure_kernel_view_dirty)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:secure_kernel_view] step %d: validate recorded world state\n", r.steps);
  if (!(w.trust.guest_ac_view_clean)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "secure_kernel_view completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "secure_kernel_view", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::secure_kernel_view
