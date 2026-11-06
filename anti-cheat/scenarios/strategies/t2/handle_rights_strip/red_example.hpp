#pragma once

// Red simulation for handle_rights_strip. It plants the kernel-attach read
// scar: entity data read through a KeStackAttachProcess-style context switch
// that never opens a handle, so ObRegisterCallbacks-style handle-right strips
// have nothing to downgrade. This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::handle_rights_strip {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::handle_rights_strip
