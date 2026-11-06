#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::stack_spoof_syscall {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int call_depth = 0; bool ret_forged = false; };
RedResult apply(sim::World& w);
}  // namespace examples::stack_spoof_syscall
