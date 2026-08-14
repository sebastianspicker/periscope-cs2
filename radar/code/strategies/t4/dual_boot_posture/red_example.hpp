#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::dual_boot_posture {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::dual_boot_posture
