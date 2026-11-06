#pragma once
// Lab BLUE: spectator feed / delayed origin residual.
#include "sim/world.hpp"
#include <string>
namespace examples::spectator_feed {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  bool feed = false;
  bool delayed_origin = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::spectator_feed
