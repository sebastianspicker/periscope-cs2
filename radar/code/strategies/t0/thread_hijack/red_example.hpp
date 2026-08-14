#pragma once

// Red simulation for thread_hijack. It plants the observable scar: hijacked game thread.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::thread_hijack {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::thread_hijack
