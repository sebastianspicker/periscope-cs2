#pragma once

// Red simulation for handle_hide_on_enum. It plants the observable scar: handle hidden during enumeration.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::handle_hide_on_enum {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::handle_hide_on_enum
