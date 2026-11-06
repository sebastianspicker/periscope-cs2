#include "red_example.hpp"
#include <sstream>
namespace examples::crosshair_helper {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("crosshair-helper.exe");
  w.crosshair_helper_active = true;
  w.sniper_crosshair_force = true;
  w.recoil_crosshair = true;
  // Explicitly not triggerbot / soft-aim residuals.
  w.triggerbot_active = false;
  r.achieved = w.crosshair_helper_active &&
               (w.sniper_crosshair_force || w.recoil_crosshair);
  std::ostringstream oss;
  oss << "crosshair_helper red sniper_xh=" << (w.sniper_crosshair_force ? 1 : 0)
      << " recoil_xh=" << (w.recoil_crosshair ? 1 : 0)
      << " (not triggerbot)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::crosshair_helper
