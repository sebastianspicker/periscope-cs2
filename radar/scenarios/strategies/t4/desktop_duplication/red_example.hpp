#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::desktop_duplication {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::desktop_duplication
