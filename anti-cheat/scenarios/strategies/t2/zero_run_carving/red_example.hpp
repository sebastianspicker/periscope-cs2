#pragma once

// Red simulation for zero_run_carving. It verifies zero pages inside the
// target image, places code there without any allocation API, and leaves the
// placement unreserved in the VAD manager - the exploitable gap blue audits.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::zero_run_carving {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::zero_run_carving
