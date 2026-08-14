#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::packet_loss_disambig {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::packet_loss_disambig
