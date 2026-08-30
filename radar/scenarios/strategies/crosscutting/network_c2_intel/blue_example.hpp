#pragma once
// Lab BLUE for `network_c2_intel` (theme multi-reason, Simulated).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::network_c2_intel {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
  bool handle_hit=false; bool intel_hit=false;
};
BlueResult detect(sim::World& w);
}  // namespace examples::network_c2_intel
