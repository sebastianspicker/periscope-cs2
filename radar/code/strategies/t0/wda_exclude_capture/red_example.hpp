#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::wda_exclude_capture {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; bool wda_applied = false; bool capture_excluded = false; };
RedResult apply(sim::World& w);
}  // namespace examples::wda_exclude_capture
