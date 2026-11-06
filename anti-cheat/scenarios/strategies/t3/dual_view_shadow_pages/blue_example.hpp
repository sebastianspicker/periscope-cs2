#pragma once

// Blue simulation for dual_view_shadow_pages. It probes cross-view
// consistency with timing, TLB/ITC effects, and self-reads; a dual-view
// shadow cannot keep every probe honest at once.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dual_view_shadow_pages {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::dual_view_shadow_pages
