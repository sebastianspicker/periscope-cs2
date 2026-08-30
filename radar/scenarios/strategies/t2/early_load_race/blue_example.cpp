// BLUE example implementation for strategy `early_load_race`.
// Multi-reason sensors: non-AC boot-start, pre-AC load order, degraded early-boot trust.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::early_load_race {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  bool saw_boot_start = false;
  bool saw_early_order = false;
  for (const auto& d : w.drivers) {
    if (!d.is_ac && d.boot_start) {
      saw_boot_start = true;
    }
    if (!d.is_ac && d.load_order < w.ac_driver_load_order) {
      saw_early_order = true;
    }
  }

  support::add_signal(r.signals, r.reasons, saw_boot_start,
                      "non-AC boot-start driver");
  support::add_signal(r.signals, r.reasons, saw_early_order,
                      "driver loads before anti-cheat");
  support::add_signal(r.signals, r.reasons,
                      !w.trust.elam_enabled || !w.trust.secure_launch,
                      "early boot protection degraded");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "early_load_race signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 early_load_race] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "early_load_race", "Audit boot driver ordering.");
  return detect(w);
}

}  // namespace examples::early_load_race
