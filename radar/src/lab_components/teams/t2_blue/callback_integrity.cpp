// callback_integrity.cpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_blue/callback_integrity.hpp"

#include <sstream>

namespace t2_blue {

// CallbackIntegrity::CallbackIntegrity: Blue auditor for process/thread/image callbacks.
CallbackIntegrity::CallbackIntegrity(ac::ITelemetrySink& sink) : sink_(sink) {
  // Default healthy baseline.
  baseline_ = {4, 3, true, 3, true, 2, true};
  has_baseline_ = true;
}

// CallbackIntegrity::set_baseline: Set baseline on this lab unit.
void CallbackIntegrity::set_baseline(CallbackSnapshot baseline) {
  baseline_ = baseline;
  has_baseline_ = true;
}

// CallbackIntegrity::audit: Audit callback counts/shadow vs expected baseline.
void CallbackIntegrity::audit(const CallbackSnapshot& now) {
  if (!has_baseline_) {
    return;
  }
  const bool degraded =
      now.process_notify_count < baseline_.process_notify_count ||
      now.image_notify_count < baseline_.image_notify_count ||
      (baseline_.expected_ac_callback_present &&
       !now.expected_ac_callback_present) ||
      (baseline_.object_present && !now.object_present) ||
      (baseline_.minifilter_present && !now.minifilter_present);

  last_degraded_ = degraded;
  if (!degraded) {
    return;
  }
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::Generic,
      .related_tier = ac::Tier::T2_KernelByovd,
      .subject_pid = 0,
      .object_pid = 0,
      .detail = "callback_integrity_degraded",
      .risk_delta = 4.0,
  });
}

// CallbackIntegrity::capture_world: Capture callback baseline snapshot from World.
CallbackSnapshot CallbackIntegrity::capture_world(const sim::World& w) const {
  CallbackSnapshot s;
  // Use sample_callbacks for shadow-aware view when applicable.
  std::size_t pn = 0, in = 0;
  bool ac = false;
  w.sample_callbacks(pn, in, ac);
  s.process_notify_count = pn;
  s.image_notify_count = in;
  s.expected_ac_callback_present = ac;
  s.object_callbacks = w.object_callbacks;
  s.object_present = w.object_callbacks_present;
  s.minifilter = w.minifilter_callbacks;
  s.minifilter_present = w.minifilter_present;
  return s;
}

// CallbackIntegrity::audit_world: Audit callbacks from live World state.
CallbackAuditResult CallbackIntegrity::audit_world(const sim::World& w) {
  CallbackAuditResult r;
  auto now = capture_world(w);
  audit(now);
  r.degraded = last_degraded_;
  r.shadow_active = w.callback_shadow_active;
  r.risk = r.degraded ? 4.0 : 0.0;
  if (r.shadow_active) {
    r.risk += 1.5;
    // Shadow is itself a scar even if probe lists look restored.
  }
  std::ostringstream oss;
  oss << "callbacks pn=" << now.process_notify_count
      << " in=" << now.image_notify_count
      << " ac=" << (now.expected_ac_callback_present ? 1 : 0)
      << " degraded=" << (r.degraded ? 1 : 0)
      << " shadow=" << (r.shadow_active ? 1 : 0);
  r.detail = oss.str();
  if (r.shadow_active && !r.degraded) {
    sink_.emit({ac::EventKind::Generic, ac::Tier::T2_KernelByovd, 0, 0,
                "callback_shadow_active", 1.5});
  }
  return r;
}

}  // namespace t2_blue
