#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::batch_read_obfuscation {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int reads_performed = 0; bool shuffled = false; bool jittered = false; };
RedResult apply(sim::World& w);
}  // namespace examples::batch_read_obfuscation
