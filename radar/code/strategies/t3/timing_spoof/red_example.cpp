#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::timing_spoof {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:timing_spoof] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:timing_spoof] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("timing_spoof-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:timing_spoof] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.timing_spoofed = true; w.trust.cpuid_latency_ns = 100.0; w.trust.baseline_latency_ns = 260.0;
  if (!(w.trust.timing_spoofed)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:timing_spoof] step %d: validate recorded world state\n", r.steps);
  if (!(w.trust.baseline_latency_ns > w.trust.cpuid_latency_ns * 2.0)) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "timing_spoof completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "timing_spoof", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::timing_spoof
