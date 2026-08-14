#pragma once
// Lab BLUE for `account_graph` (theme multi-reason, Simulated).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::account_graph {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
  int linked = 0;
};
BlueResult detect(sim::World& w);
}  // namespace examples::account_graph
