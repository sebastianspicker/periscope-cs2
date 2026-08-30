#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::dual_boot_posture {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:dual_boot_posture] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:dual_boot_posture] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("dual_boot_posture-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:dual_boot_posture] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.dual_boot_profile = true; w.trust.boot_pcr_profile = "alternate_lab";
  if (!(w.trust.dual_boot_profile)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:dual_boot_posture] step %d: validate recorded world state\n", r.steps);
  if (!(w.trust.boot_pcr_profile != "known_good")) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "dual_boot_posture completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::dual_boot_posture
