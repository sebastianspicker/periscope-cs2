#pragma once

// Red simulation for printwindow. It plants the observable scar: PrintWindow capture.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::printwindow {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::printwindow
