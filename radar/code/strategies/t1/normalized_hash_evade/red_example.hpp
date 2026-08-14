#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::normalized_hash_evade {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; bool hash_normalized = false; int sections_altered = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::normalized_hash_evade
