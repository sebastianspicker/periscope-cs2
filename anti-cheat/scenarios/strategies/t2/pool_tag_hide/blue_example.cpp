// BLUE example implementation for strategy `pool_tag_hide`.
// Multi-reason sensors: pool tag anomaly, obscure driver, shared mem-rw footprint.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::pool_tag_hide {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, w.pool_tag_anomaly,
                      "pool allocation tag anomaly");
  support::add_signal(r.signals, r.reasons,
                      support::has_driver(w, "pool-obscure.sys"),
                      "obscure driver associated with anomalous pool use");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "memory provider shares kernel footprint");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "pool_tag_hide signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 pool_tag_hide] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "pool_tag_hide",
            "Score pool allocation inconsistencies.");
  return detect(w);
}

}  // namespace examples::pool_tag_hide
