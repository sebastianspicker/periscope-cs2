#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::dynamic_api_table {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int apis_resolved = 0; bool peb_walked = false; bool eat_resolved = false; };
RedResult apply(sim::World& w);
}  // namespace examples::dynamic_api_table
