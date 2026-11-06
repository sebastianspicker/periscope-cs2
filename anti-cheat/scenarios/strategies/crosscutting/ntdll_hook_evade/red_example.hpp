#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::ntdll_hook_evade {

struct RedResult {
  bool achieved;
  int steps;
  int hooks_found;
  bool used_clean_copy;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::ntdll_hook_evade
