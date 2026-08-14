#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::hw_monitor_disguise {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; bool osd_active = false; bool rtss_hijack = false; };
RedResult apply(sim::World& w);
}  // namespace examples::hw_monitor_disguise
