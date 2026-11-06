#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::pcie_peer_dma {
struct BlueResult { bool detected, mitigated; int signals; double risk; std::vector<std::string> reasons; std::string detail; };
BlueResult detect(sim::World& w);
}
