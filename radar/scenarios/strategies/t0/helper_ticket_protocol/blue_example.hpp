#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::helper_ticket_protocol {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool shared_section_found = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::helper_ticket_protocol
