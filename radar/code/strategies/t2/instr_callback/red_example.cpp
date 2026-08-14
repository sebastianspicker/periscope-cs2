// RED example implementation for strategy `instr_callback`.
// Multi-step World scars: install instrumentation callback + instrumented-client.exe.

#include "red_example.hpp"

#include <cstdio>

namespace examples::instr_callback {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "instrumentation callback precondition failed";

  std::printf("[red:instr_callback] require instrumentation callback not already set\n");
  if (w.instrumentation_callback) {
    return r;
  }

  std::printf("[red:instr_callback] enable process instrumentation callback scar\n");
  w.instrumentation_callback = true;
  if (!w.instrumentation_callback) {
    r.detail = "failed to set instrumentation_callback";
    return r;
  }
  ++r.steps;

  std::printf("[red:instr_callback] spawn instrumented-client.exe actor\n");
  const auto actor = w.spawn("instrumented-client.exe");
  if (!w.proc(actor)) {
    r.detail = "spawn of instrumented-client.exe failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:instr_callback] verify callback residual and actor presence\n");
  if (!w.instrumentation_callback || !w.proc(actor)) {
    r.detail = "instrumentation callback residual verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "process instrumentation callback and actor verified";
  w.note(r.detail);
  std::printf("[red:instr_callback] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "instr_callback",
         "Install instrumentation callback state.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::instr_callback
