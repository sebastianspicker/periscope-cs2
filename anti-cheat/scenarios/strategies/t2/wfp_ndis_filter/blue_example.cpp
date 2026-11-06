// BLUE example implementation for strategy `wfp_ndis_filter`.
// Multi-reason sensors: filter active, telemetry-filter.sys, correlated blind telemetry.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::wfp_ndis_filter {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, w.wfp_ndis_filter,
                      "WFP/NDIS filter state active");
  support::add_signal(r.signals, r.reasons,
                      support::has_driver(w, "telemetry-filter.sys"),
                      "untrusted packet filter driver loaded");
  support::add_signal(r.signals, r.reasons, !w.etw_enabled || w.etw_ti_blind,
                      "telemetry filter correlated with blind telemetry");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "wfp_ndis_filter signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 wfp_ndis_filter] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "wfp_ndis_filter",
            "Inspect network filter registrations.");
  return detect(w);
}

}  // namespace examples::wfp_ndis_filter
