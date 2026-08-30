#pragma once

// Red simulation for handle_minimize. It plants the observable scar: brief VM_READ handle.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::handle_minimize {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::handle_minimize
