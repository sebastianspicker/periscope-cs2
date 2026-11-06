#pragma once

// Red simulation for process_cooccurrence. It plants the observable scar: co-resident radar process.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::process_cooccurrence {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::process_cooccurrence
