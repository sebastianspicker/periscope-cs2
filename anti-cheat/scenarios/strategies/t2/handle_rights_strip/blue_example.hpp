#pragma once

// Blue simulation for handle_rights_strip. It arms an ObRegisterCallbacks-style
// handle-rights strip on the protected game pid, then detects the attach-read
// scar with independent reasons before reporting a risk-scored verdict.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::handle_rights_strip {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::handle_rights_strip
