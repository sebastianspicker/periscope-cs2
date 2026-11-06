#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::peb_spoof_hide {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; bool dbg_cleared = false; bool flag_cleared = false; };
RedResult apply(sim::World& w);
}  // namespace examples::peb_spoof_hide
