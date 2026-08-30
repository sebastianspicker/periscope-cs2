// BLUE example implementation for strategy `minifilter_strip`.
// Multi-reason sensors: missing registration, zero callbacks, correlated kernel RW.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::minifilter_strip {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, !w.minifilter_present,
                      "minifilter registration missing");
  support::add_signal(r.signals, r.reasons, w.minifilter_callbacks == 0,
                      "no minifilter callbacks registered");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "kernel provider correlated with removal");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "minifilter_strip signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 minifilter_strip] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "minifilter_strip",
            "Check minifilter registration and callbacks.");
  return detect(w);
}

}  // namespace examples::minifilter_strip
