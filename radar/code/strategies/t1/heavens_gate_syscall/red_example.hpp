#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::heavens_gate_syscall {

struct RedResult {
  bool achieved;
  int steps;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::heavens_gate_syscall
