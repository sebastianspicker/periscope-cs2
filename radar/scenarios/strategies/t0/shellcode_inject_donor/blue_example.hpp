#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::shellcode_inject_donor {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool donor_injection_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::shellcode_inject_donor
