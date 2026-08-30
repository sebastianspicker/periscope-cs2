#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::forensic_cleanup_exit {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int steps_completed = 0; bool prefetch_cleared = false; bool recent_cleared = false; };
RedResult apply(sim::World& w);
}  // namespace examples::forensic_cleanup_exit
