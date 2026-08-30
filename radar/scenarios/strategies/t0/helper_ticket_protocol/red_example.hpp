#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::helper_ticket_protocol {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; std::uint32_t ticket_id = 0; bool shared_mem = false; bool claimed = false; };
RedResult apply(sim::World& w);
}  // namespace examples::helper_ticket_protocol
