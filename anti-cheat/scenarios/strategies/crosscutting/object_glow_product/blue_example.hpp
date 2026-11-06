#pragma once
// Lab BLUE: multi-class object glow residual (≠ fuse timer 34).
#include "sim/world.hpp"
#include <string>
namespace examples::object_glow_product {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int objects = 0;
  int class_count = 0;
  bool multi_class = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::object_glow_product
