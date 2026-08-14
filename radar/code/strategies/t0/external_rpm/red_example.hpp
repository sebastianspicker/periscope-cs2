#pragma once

// Red simulation for external_rpm. It plants the observable scar: foreign VM_READ channel.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::external_rpm {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::external_rpm
