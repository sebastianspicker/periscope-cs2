// BLUE example implementation for strategy `registry_notify_strip`.
// Multi-reason sensors: missing registration, zero count, correlated kernel RW.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::registry_notify_strip {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, !w.registry_notify_present,
                      "registry notification registration missing");
  support::add_signal(r.signals, r.reasons, w.registry_notify == 0,
                      "registry notification callback count is zero");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "kernel provider accompanies callback loss");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "registry_notify_strip signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 registry_notify_strip] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "registry_notify_strip",
            "Audit registry notification callbacks.");
  return detect(w);
}

}  // namespace examples::registry_notify_strip
