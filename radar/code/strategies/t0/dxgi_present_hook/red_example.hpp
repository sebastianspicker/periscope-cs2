#pragma once

// Red simulation for dxgi_present_hook. It plants the observable scar: DXGI Present detour.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::dxgi_present_hook {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::dxgi_present_hook
