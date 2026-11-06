#pragma once

// Red simulation for section_entropy_probe (166). The loader resolves the
// address registry offline into an encrypted non-image section and decodes it
// into registers only, so the data section is never patched. Sim only.
#include "sim/world.hpp"

#include <string>

namespace examples::section_entropy_probe {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::section_entropy_probe
