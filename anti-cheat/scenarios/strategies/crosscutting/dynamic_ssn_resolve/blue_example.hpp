#pragma once

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::dynamic_ssn_resolve {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

BlueResult detect(sim::World& w);

}  // namespace examples::dynamic_ssn_resolve
