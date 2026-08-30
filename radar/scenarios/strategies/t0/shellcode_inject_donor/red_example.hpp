#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::shellcode_inject_donor {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; std::uint32_t donor_pid = 0; bool obfuscated = false; };
RedResult apply(sim::World& w);
}  // namespace examples::shellcode_inject_donor
