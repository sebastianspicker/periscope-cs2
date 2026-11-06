// BLUE example implementation for strategy `scm_service`.
// Multi-reason sensors: kernel service creation, scm-lab.sys image, mem-rw capability.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::scm_service {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  const bool kernel_service = std::any_of(
      w.services.begin(), w.services.end(),
      [](const auto& s) { return s.kernel_driver; });

  support::add_signal(r.signals, r.reasons, kernel_service,
                      "kernel driver service creation observed");
  support::add_signal(r.signals, r.reasons, support::has_driver(w, "scm-lab.sys"),
                      "service image loaded as driver");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "new service supplies memory capability");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "scm_service signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 scm_service] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "scm_service",
            "Correlate SCM and driver load telemetry.");
  return detect(w);
}

}  // namespace examples::scm_service
