#pragma once

// Red simulation for escalation_cost_curve. Red walks the access rungs
// (RPM -> syscall -> kernel driver), each rung leaving distinct persistent
// system mutations: testsigning flag, HVCI/blocklist registry toggles,
// pending reboot. This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::escalation_cost_curve {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::escalation_cost_curve
