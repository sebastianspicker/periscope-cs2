#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::packet_loss_disambig {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:packet_loss_disambig] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:packet_loss_disambig] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("packet_loss_disambig-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:packet_loss_disambig] step %d: apply isolated strategy residual\n", r.steps);
  w.packet_loss_faked = true; w.lag_switch_active = false;
  if (!(w.packet_loss_faked)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:packet_loss_disambig] step %d: validate recorded world state\n", r.steps);
  if (!(!w.lag_switch_active)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "packet_loss_disambig completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::packet_loss_disambig
