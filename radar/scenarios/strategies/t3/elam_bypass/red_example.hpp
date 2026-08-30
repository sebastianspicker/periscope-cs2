#pragma once
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include <string>

namespace examples::elam_bypass {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
RedResult run_red(sim::World& w, sim::Narrator& n);
}  // namespace examples::elam_bypass
