#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::dynamic_ssn_resolve {

struct RedResult {
  bool achieved;
  int steps;
  int resolved_ssn_count;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::dynamic_ssn_resolve
