// RED example implementation for strategy `etw_ti_blind`.
// Multi-step World scars: blind ETW TI and disable the provider, then verify.

#include "red_example.hpp"

#include <cstdio>

namespace examples::etw_ti_blind {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "ETW TI precondition failed: telemetry already disabled";

  std::printf("[red:etw_ti_blind] require ETW currently enabled and not already blind\n");
  if (!w.etw_enabled || w.etw_ti_blind) {
    return r;
  }

  std::printf("[red:etw_ti_blind] mark ETW Threat Intelligence blind\n");
  w.etw_ti_blind = true;
  if (!w.etw_ti_blind) {
    r.detail = "failed to set etw_ti_blind scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_ti_blind] disable the ETW provider surface\n");
  w.etw_enabled = false;
  if (w.etw_enabled) {
    r.detail = "failed to clear etw_enabled";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_ti_blind] verify blind + disabled residual\n");
  if (w.etw_enabled || !w.etw_ti_blind) {
    r.detail = "ETW TI blind state verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "ETW Threat Intelligence blind state verified";
  w.note(r.detail);
  std::printf("[red:etw_ti_blind] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "etw_ti_blind", "Disable ETW TI collection in the lab.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::etw_ti_blind
