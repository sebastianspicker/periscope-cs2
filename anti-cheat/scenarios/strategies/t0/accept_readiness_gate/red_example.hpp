#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::accept_readiness_gate {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int conditions_met = 0; bool gate_open = false; bool session_ready = false; };
RedResult apply(sim::World& w);
}  // namespace examples::accept_readiness_gate
