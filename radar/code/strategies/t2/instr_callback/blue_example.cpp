// BLUE example implementation for strategy `instr_callback`.
// Multi-reason sensors: callback installed, instrumented client, optional TI pairing.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::instr_callback {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  const bool instrumented_client = std::any_of(
      w.processes.begin(), w.processes.end(),
      [](const auto& entry) { return entry.second.name == "instrumented-client.exe"; });

  support::add_signal(r.signals, r.reasons, w.instrumentation_callback,
                      "process instrumentation callback installed");
  support::add_signal(r.signals, r.reasons, instrumented_client,
                      "instrumented client process observed");
  support::add_signal(r.signals, r.reasons, w.etw_ti_blind,
                      "instrumentation paired with TI blind state");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "instr_callback signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 instr_callback] BLUE: %s\n", reason.c_str());
  }
  return r;
}

bool mitigate(sim::World& w) {
  if (!detect(w).detected) {
    return false;
  }
  w.ranked_access_denied = true;
  return true;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "instr_callback",
            "Inspect process instrumentation state.");
  return detect(w);
}

}  // namespace examples::instr_callback
