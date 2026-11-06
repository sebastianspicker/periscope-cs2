#pragma once

// Blue simulation for zero_run_carving. It audits VAD-manager reservations
// against PTE-valid pages and scores the unreserved-placement gap with
// independent reasons before reporting a risk-scored verdict.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::zero_run_carving {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::zero_run_carving
