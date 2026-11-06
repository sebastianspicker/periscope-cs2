// BLUE example implementation for strategy `byovd`.
// Multi-reason sensors: untrusted mem-rw driver, known-bad hash, mem-rw device.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::byovd {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  const bool known_bad = std::any_of(
      w.drivers.begin(), w.drivers.end(),
      [](const auto& d) { return d.byovd_known_bad; });
  const bool mem_device = std::any_of(
      w.devices.begin(), w.devices.end(),
      [](const auto& d) { return d.mem_rw_ioctl; });

  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "memory-capable non-AC driver loaded");
  support::add_signal(r.signals, r.reasons, known_bad,
                      "known vulnerable driver hash");
  support::add_signal(r.signals, r.reasons, mem_device,
                      "memory read/write device exposed");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "byovd signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 byovd] BLUE: %s\n", reason.c_str());
  }
  return r;
}

bool mitigate(sim::World& w) {
  if (!detect(w).detected) {
    return false;
  }
  w.byovd_policy_block = true;
  w.ranked_access_denied = true;
  return true;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "byovd",
            "Check driver, blocklist, and device telemetry.");
  auto r = detect(w);
  std::printf("[blue/byovd] signals=%d risk=%.2f\n", r.signals, r.risk);
  return r;
}

}  // namespace examples::byovd
