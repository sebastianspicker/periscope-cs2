// BLUE example implementation for strategy `dkom_hide`.
// Multi-reason sensors: weak/full inventory divergence, hide flag, correlated kernel RW.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::dkom_hide {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  const auto weak = w.list_processes(true);
  const auto full = w.list_processes(false);

  support::add_signal(r.signals, r.reasons, full.size() > weak.size(),
                      "strong process walk exceeds weak enumeration");
  support::add_signal(
      r.signals, r.reasons,
      std::any_of(full.begin(), full.end(),
                  [](const auto& p) { return p.hidden_from_weak_enum; }),
      "process marked hidden from weak enumeration");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "kernel memory provider correlated with hide attempt");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "dkom_hide signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 dkom_hide] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "dkom_hide",
            "Compare independent process inventories.");
  return detect(w);
}

}  // namespace examples::dkom_hide
