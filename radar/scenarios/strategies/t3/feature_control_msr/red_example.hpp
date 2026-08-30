#pragma once
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include <string>

namespace examples::feature_control_msr {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
RedResult run_red(sim::World& w, sim::Narrator& n);
}  // namespace examples::feature_control_msr
