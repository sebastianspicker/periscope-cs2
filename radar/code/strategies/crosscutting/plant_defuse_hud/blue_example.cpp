// BLUE: multi-reason plant/defuse feasibility HUD residual (signals >= 2).

#include "blue_example.hpp"
#include <sstream>

namespace examples::plant_defuse_hud {

BlueResult detect(sim::World& w) {
  BlueResult r;

  // Independent residual 1: plant HUD alert with known remaining time.
  r.plant_alert = w.plant_hud_alert_active && w.plant_time_remaining_known >= 0.f;
  if (r.plant_alert) {
    r.reasons.emplace_back("plant_hud_alert with known time_remaining");
  }

  // Independent residual 2: plant feasibility flag before round end.
  if (w.plant_feasible_before_round_end) {
    r.reasons.emplace_back("plant_feasible_before_round_end asserted");
  }

  // Independent residual 3: defuse-window alert (kit timing intel).
  r.defuse_alert = w.defuse_window_alert_active;
  if (r.defuse_alert) {
    r.reasons.emplace_back("defuse_window_alert_active");
  }

  // Independent residual 4: HUD product without fuse-timer bomb product.
  if ((w.plant_hud_alert_active || w.defuse_window_alert_active) &&
      !w.bomb_intel_product) {
    r.reasons.emplace_back("timing HUD without bomb_intel fuse product");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  r.mitigated = r.detected && r.plant_alert && r.defuse_alert;
  if (r.mitigated) {
    w.plant_hud_alert_active = false;
    w.defuse_window_alert_active = false;
    w.plant_feasible_before_round_end = false;
    w.plant_time_remaining_known = -1.f;
    w.ranked_access_denied = true;
  }

  std::ostringstream oss;
  oss << "plant_defuse_hud blue signals=" << r.signals
      << " plant_alert=" << (r.plant_alert ? 1 : 0)
      << " defuse_alert=" << (r.defuse_alert ? 1 : 0)
      << " mitigated=" << (r.mitigated ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::plant_defuse_hud
