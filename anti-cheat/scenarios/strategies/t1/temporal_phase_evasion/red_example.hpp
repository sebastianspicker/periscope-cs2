#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::temporal_phase_evasion {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int phase_transitions = 0; int ticks_in_phase = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::temporal_phase_evasion
