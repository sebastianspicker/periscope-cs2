// BLUE: score bomb timer product; mitigate by stripping round intel to clients.

#include "blue_example.hpp"

#include <sstream>

namespace examples::bomb_round_intel {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.bomb_product = w.bomb_intel_product;
  r.timer_leak = w.bomb_fuse_known >= 0.f && !w.bomb_site_known.empty();
  // Extra residual: defuse progress known without being the defuser (product).
  const bool defuse_leak = w.bomb_defuse_known && w.bomb_defuse_progress_known > 0.f;
  r.detected = r.bomb_product && r.timer_leak;
  r.mitigated = r.detected && (defuse_leak || r.timer_leak);
  if (r.mitigated) {
    // Structural: stop advertising full bomb state on the client product path.
    w.bomb_intel_product = false;
    w.bomb_fuse_known = -1.f;
    w.bomb_defuse_known = false;
    w.bomb_defuse_progress_known = 0.f;
    w.bomb_site_known.clear();
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "bomb_round_intel blue product=" << (r.bomb_product ? 1 : 0)
      << " timer_leak=" << (r.timer_leak ? 1 : 0)
      << " defuse_leak=" << (defuse_leak ? 1 : 0)
      << " mitigated=" << (r.mitigated ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

BlueResult detect_with_scenario(sim::World& w, const fps::Scenario& sc) {
  (void)sc;
  return detect(w);
}

}  // namespace examples::bomb_round_intel
