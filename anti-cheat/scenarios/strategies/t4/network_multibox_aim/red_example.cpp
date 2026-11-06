#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::network_multibox_aim {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:network_multibox_aim] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:network_multibox_aim] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("network_multibox_aim-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:network_multibox_aim] step %d: apply isolated strategy residual\n", r.steps);
  w.multibox_net_aim = true; w.multibox_remote_pid = w.spawn("multibox-secondary.exe"); w.multibox_aim_samples = 8; w.multibox_input_desync = true;
  if (!(w.multibox_net_aim)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:network_multibox_aim] step %d: validate recorded world state\n", r.steps);
  if (!(w.multibox_input_desync)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "network_multibox_aim completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::network_multibox_aim
