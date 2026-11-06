// BLUE example implementation for strategy `object_callback_strip`.
// Multi-reason sensors: count vs ground truth, missing registration, expected baseline.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::object_callback_strip {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons,
                      w.object_callbacks < w.object_callbacks_true,
                      "object callback count below ground truth");
  support::add_signal(r.signals, r.reasons, !w.object_callbacks_present,
                      "object callback registration missing");
  support::add_signal(r.signals, r.reasons, w.object_callbacks_true_present,
                      "ground truth still expects callbacks");

  // Require multi-signal consensus (same as prior one-liner semantics).
  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "object_callback_strip signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 object_callback_strip] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "object_callback_strip",
            "Validate object callback baseline.");
  return detect(w);
}

}  // namespace examples::object_callback_strip
