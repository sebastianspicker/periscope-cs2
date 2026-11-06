#pragma once
// Lab BLUE: flash alpha strip residual (presentation, not ESP).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::no_flash {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool fx_strip = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::no_flash
