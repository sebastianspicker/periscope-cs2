#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::external_clone_display {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::external_clone_display
