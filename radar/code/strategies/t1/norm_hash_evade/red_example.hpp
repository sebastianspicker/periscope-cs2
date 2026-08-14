#pragma once

#include "sim/world.hpp"
#include <string>

namespace examples::norm_hash_evade {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}
