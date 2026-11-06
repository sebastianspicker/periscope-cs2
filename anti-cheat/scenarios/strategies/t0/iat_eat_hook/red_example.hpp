#pragma once

// Red simulation for iat_eat_hook. It plants the observable scar: IAT and EAT detours.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::iat_eat_hook {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::iat_eat_hook
