#pragma once
// Lab BLUE for `report_velocity` (multi-reason cluster detect).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::report_velocity {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
  int high_report = 0;
};
BlueResult detect(sim::World& w);
}  // namespace examples::report_velocity
