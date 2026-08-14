#pragma once

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::vmexit_keylog_capture {

struct BlueResult {
  bool detected;
  bool mitigated;
  int signals;
  double risk;
  std::vector<std::string> reasons;
  std::string detail;
};

BlueResult detect(sim::World& w);

}  // namespace examples::vmexit_keylog_capture
