#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::noscope_inaccuracy_viz {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::noscope_inaccuracy_viz
