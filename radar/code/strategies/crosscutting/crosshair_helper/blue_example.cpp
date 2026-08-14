#include "blue_example.hpp"
#include <sstream>
namespace examples::crosshair_helper {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.crosshair_helper_active) ++reasons;
  if (w.sniper_crosshair_force) ++reasons;
  if (w.recoil_crosshair) ++reasons;
  // Distinct: do not require triggerbot_active.
  r.detected = reasons >= 2 && !w.triggerbot_active;
  r.mitigated = reasons >= 2;
  if (r.mitigated) {
    w.crosshair_helper_active = false;
    w.sniper_crosshair_force = false;
    w.recoil_crosshair = false;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "crosshair_helper blue reasons=" << reasons
      << " not_trigger=" << (w.triggerbot_active ? 0 : 1);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::crosshair_helper
