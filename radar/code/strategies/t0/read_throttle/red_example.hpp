#pragma once

// Red simulation for read_throttle. It plants the observable scar: brief, throttled remote read.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::read_throttle {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::read_throttle
