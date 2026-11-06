#pragma once
// Lab BLUE for `input_synthesis` (theme multi-reason, Simulated).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::input_synthesis {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::input_synthesis
