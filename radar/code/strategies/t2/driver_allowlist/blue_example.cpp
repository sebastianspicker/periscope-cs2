// BLUE example implementation for strategy `driver_allowlist`.
// Multi-reason sensors: allowlist policy, non-allowlisted mem-rw, approved device exposure.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::driver_allowlist {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, w.ranked_strict_driver_allowlist,
                      "ranked driver allowlist enforced");

  bool missing_from_allowlist = false;
  for (const auto& d : w.drivers) {
    const bool allowed =
        std::find(w.driver_allowlist_sha.begin(), w.driver_allowlist_sha.end(),
                  d.sha256) != w.driver_allowlist_sha.end();
    if (!d.is_ac && d.provides_mem_rw && !allowed) {
      missing_from_allowlist = true;
      break;
    }
  }
  support::add_signal(r.signals, r.reasons, missing_from_allowlist,
                      "memory-capable driver missing from allowlist");
  support::add_signal(r.signals, r.reasons,
                      support::has_device(w, "\\\\.\\ApprovedMemRw"),
                      "approved identity exposes memory device");

  // Prior one-liner required multi-signal consensus (>=2).
  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "driver_allowlist signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 driver_allowlist] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "driver_allowlist",
            "Validate hashes and device capabilities.");
  return detect(w);
}

}  // namespace examples::driver_allowlist
