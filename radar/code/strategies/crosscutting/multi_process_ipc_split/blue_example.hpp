#pragma once

#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::multi_process_ipc_split {
struct BlueResult {
  bool detected, mitigated;
  int signals;
  double risk;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::multi_process_ipc_split
