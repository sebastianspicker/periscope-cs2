#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::dkom_token_steal {

struct RedResult {
  bool achieved;
  int steps;
  bool handle_bypasses_acls;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::dkom_token_steal
