// BLUE example implementation for strategy `etw_ti_blind`.
// Multi-reason sensors: TI blind flag, ETW disabled, correlated secondary tamper surface.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::etw_ti_blind {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, w.etw_ti_blind,
                      "ETW Threat Intelligence marked blind");
  support::add_signal(r.signals, r.reasons, !w.etw_enabled,
                      "ETW provider disabled");
  support::add_signal(r.signals, r.reasons,
                      w.instrumentation_callback || w.wfp_ndis_filter,
                      "secondary tamper surface accompanies telemetry loss");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "etw_ti_blind signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 etw_ti_blind] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "etw_ti_blind",
            "Check TI health and independent tamper scars.");
  return detect(w);
}

}  // namespace examples::etw_ti_blind
