#pragma once

// Red simulation for internal_inject. It plants the observable scar: foreign module and execution thread.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::internal_inject {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::internal_inject
