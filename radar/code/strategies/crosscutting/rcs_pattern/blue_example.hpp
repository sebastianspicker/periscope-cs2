#pragma once
// Lab BLUE: near-perfect RCS pattern fit residual.
#include "sim/world.hpp"
#include <string>
namespace examples::rcs_pattern {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int fit_samples = 0;
  float max_err = 0.f;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::rcs_pattern
