#pragma once

// Red simulation for module_integrity. It plants the observable scar: patched executable module.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::module_integrity {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::module_integrity
