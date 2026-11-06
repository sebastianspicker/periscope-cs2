// RED example implementation for strategy `callback_shadow`.
// Multi-step World scars: snapshot true callbacks then enable probe-facing shadow.

#include "red_example.hpp"

#include <cstdio>

namespace examples::callback_shadow {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "callback shadow precondition failed: no callback baseline";

  std::printf("[red:callback_shadow] require AC callback baseline present\n");
  if (!w.ac_callback_present || w.process_notify == 0) {
    return r;
  }

  std::printf("[red:callback_shadow] snapshot ground-truth then degrade live callbacks\n");
  // Capture true counts, strip live chain (real residual), then façade via shadow.
  w.process_notify_true = w.process_notify;
  w.image_notify_true = w.image_notify;
  w.ac_callback_true = w.ac_callback_present;
  if (w.process_notify_true == 0 || !w.ac_callback_true) {
    r.detail = "failed to capture callback ground truth";
    return r;
  }
  // Degrade the true/live chain so shadow can present a clean probe façade.
  w.process_notify = 1;
  w.image_notify = 1;
  w.ac_callback_present = false;
  w.process_notify_true = 1;
  w.image_notify_true = 1;
  w.ac_callback_true = false;
  ++r.steps;

  std::printf("[red:callback_shadow] enable probe-facing callback shadow\n");
  // enable_callback_shadow(true) saves current (degraded) as true and shows clean façade.
  w.enable_callback_shadow(true);
  if (!w.callback_shadow_active) {
    r.detail = "failed to enable callback_shadow_active";
    return r;
  }
  ++r.steps;

  std::printf("[red:callback_shadow] verify shadow residual and preserved snapshot\n");
  // Façade looks clean; true residual remains degraded under the shadow.
  if (!w.callback_shadow_active || w.process_notify_true != 1 ||
      w.ac_callback_true || w.process_notify < 2) {
    r.detail = "callback shadow residual verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "shadow state preserves probe-facing callback snapshot";
  w.note(r.detail);
  std::printf("[red:callback_shadow] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "callback_shadow", "Enable callback-view shadowing.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::callback_shadow
