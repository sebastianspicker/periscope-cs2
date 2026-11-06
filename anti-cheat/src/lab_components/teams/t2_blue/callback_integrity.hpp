// callback_integrity.hpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t2_red/callback_strip_sim.hpp"

#include <string>

namespace t2_blue {

// CallbackSnapshot: lab type for this educational unit.
struct CallbackSnapshot {
  std::size_t process_notify_count = 0;
  std::size_t image_notify_count = 0;
  bool expected_ac_callback_present = true;
  std::size_t object_callbacks = 0;
  bool object_present = true;
  std::size_t minifilter = 0;
  bool minifilter_present = true;
};

// CallbackAuditResult: lab type for this educational unit.
struct CallbackAuditResult {
  bool degraded = false;
  bool shadow_active = false;
  std::string detail;
  double risk = 0;
};

// CallbackIntegrity: lab type for this educational unit.
class CallbackIntegrity {
 public:
  explicit CallbackIntegrity(ac::ITelemetrySink& sink);
  void set_baseline(CallbackSnapshot baseline);
  void audit(const CallbackSnapshot& now);

  /// Capture World into snapshot / audit vs baseline (or live sample).
  CallbackAuditResult audit_world(const sim::World& w);
  CallbackSnapshot capture_world(const sim::World& w) const;

  bool last_degraded() const { return last_degraded_; }

 private:
  ac::ITelemetrySink& sink_;
  CallbackSnapshot baseline_{};
  bool has_baseline_ = false;
  bool last_degraded_ = false;
};

}  // namespace t2_blue
