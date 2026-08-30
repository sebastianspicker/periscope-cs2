#pragma once

// Red simulation for gdi_bitblt. It plants the observable scar: GDI BitBlt capture.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::gdi_bitblt {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::gdi_bitblt
