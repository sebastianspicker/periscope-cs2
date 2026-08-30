#pragma once

// BLUE: interest fog on CS-round entity truth + multi-reason channel residuals.

#include "sim/world.hpp"
#include "fps/scenario.hpp"

#include <string>
#include <vector>

namespace examples::cs_round_radar {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  int enemies_culled = 0;
  int truth_alive = 0;
  int replicated = 0;
  bool foreign_vm_read = false;
  bool fog = false;
  std::string detail;
};

// Fog cull using scenario player truth; mitigate full-origin product on World.
BlueResult detect(sim::World& w);

// Prefer this when scenario still available (CT observer at CT spawn).
BlueResult detect_with_scenario(sim::World& w, const fps::Scenario& sc);

}  // namespace examples::cs_round_radar
