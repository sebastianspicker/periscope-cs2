#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::ept_memory_hiding {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int hidden_pages = 0; bool invept_used = false; };
RedResult apply(sim::World& w);
}  // namespace examples::ept_memory_hiding
