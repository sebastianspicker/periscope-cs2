#pragma once
// Lab BLUE: multi-reason present-path overlay vs capture-path miss residual.
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::streamproof_overlay {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool present_overlay = false;
  bool capture_miss = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::streamproof_overlay
