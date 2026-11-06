#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::ept_violation_evade {

struct RedResult {
  bool achieved;
  int steps;
  int ac_scans_evaded;
  bool used_invept;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::ept_violation_evade
