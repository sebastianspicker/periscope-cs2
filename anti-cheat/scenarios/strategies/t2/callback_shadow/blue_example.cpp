// BLUE example implementation for strategy `callback_shadow`.
// Multi-reason sensors: shadow active, sampled process/image divergence, AC state drift.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::callback_shadow {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  std::size_t process_sampled = 0;
  std::size_t image_sampled = 0;
  bool ac_sampled = false;
  w.sample_callbacks(process_sampled, image_sampled, ac_sampled);

  support::add_signal(r.signals, r.reasons, w.callback_shadow_active,
                      "callback probe returns shadowed view");
  support::add_signal(r.signals, r.reasons,
                      w.callback_shadow_active &&
                          (process_sampled != w.process_notify_true ||
                           image_sampled != w.image_notify_true),
                      "sampled callback view diverges from live state");
  support::add_signal(r.signals, r.reasons,
                      w.callback_shadow_active &&
                          (ac_sampled != w.ac_callback_true),
                      "sampled AC callback state diverges");
  // Ground-truth residual: true chain degraded under the façade.
  support::add_signal(r.signals, r.reasons,
                      w.callback_shadow_active && w.process_notify_true < 2,
                      "true process-notify residual degraded under shadow");
  support::add_signal(r.signals, r.reasons,
                      w.callback_shadow_active && !w.ac_callback_true,
                      "true AC callback residual missing under shadow");
  support::add_signal(r.signals, r.reasons,
                      !w.callback_shadow_active &&
                          (process_sampled < 2 || !ac_sampled),
                      "callback chain degraded without shadow façade");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "callback_shadow signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 callback_shadow] BLUE: %s\n", reason.c_str());
  }
  return r;
}

bool mitigate(sim::World& w) {
  if (!detect(w).detected) {
    return false;
  }
  w.enable_callback_shadow(false);
  w.ranked_access_denied = true;
  return true;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "callback_shadow",
            "Cross-check live and sampled callback views.");
  auto r = detect(w);
  std::printf("[blue/callback_shadow] signals=%d risk=%.2f\n", r.signals, r.risk);
  return r;
}

}  // namespace examples::callback_shadow
