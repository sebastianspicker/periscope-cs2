#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::wda_exclude_capture {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; double risk = 0; std::vector<std::string> reasons; std::string detail; bool stream_proof_overlay = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::wda_exclude_capture
