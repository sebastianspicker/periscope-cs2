#pragma once

// Red simulation for wh_mouse_hook. It plants the observable scar: global mouse hook.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::wh_mouse_hook {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::wh_mouse_hook
