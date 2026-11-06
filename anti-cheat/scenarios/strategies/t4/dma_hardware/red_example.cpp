#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::dma_hardware {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:dma_hardware] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:dma_hardware] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("dma_hardware-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:dma_hardware] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.dma_device_present = true; w.trust.iommu_on = false;
  if (!(w.trust.dma_device_present)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:dma_hardware] step %d: validate recorded world state\n", r.steps);
  if (!(!w.trust.iommu_on)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "dma_hardware completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

}  // namespace examples::dma_hardware
