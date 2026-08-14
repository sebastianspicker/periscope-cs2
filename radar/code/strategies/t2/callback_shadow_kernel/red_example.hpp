#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::callback_shadow_kernel {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; std::size_t pn_restored = 0; std::size_t in_restored = 0; bool stripped_during_ioctl = false; };
RedResult apply(sim::World& w);
}  // namespace examples::callback_shadow_kernel
