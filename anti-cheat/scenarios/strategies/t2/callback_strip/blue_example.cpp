// BLUE example implementation for strategy `callback_strip`.
// Multi-reason sensors: process/image notify below baseline, AC callback missing.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::callback_strip {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons,
                      w.process_notify < w.process_notify_true,
                      "process notify count below baseline");
  support::add_signal(r.signals, r.reasons,
                      w.image_notify < w.image_notify_true,
                      "image notify count below baseline");
  support::add_signal(r.signals, r.reasons, !w.ac_callback_present,
                      "anti-cheat callback missing");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "callback_strip signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 callback_strip] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "callback_strip",
            "Compare callback lists to the baseline.");
  auto r = detect(w);
  std::printf("[blue/callback_strip] signals=%d risk=%.2f\n", r.signals, r.risk);
  return r;
}

}  // namespace examples::callback_strip
