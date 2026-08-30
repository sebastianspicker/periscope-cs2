#pragma once
// Lab BLUE: bomb timer / defuse intel residual beyond entity stream.
#include "sim/world.hpp"
#include "fps/scenario.hpp"
#include <string>
namespace examples::bomb_round_intel {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  bool bomb_product = false;
  bool timer_leak = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
BlueResult detect_with_scenario(sim::World& w, const fps::Scenario& sc);
}  // namespace examples::bomb_round_intel
