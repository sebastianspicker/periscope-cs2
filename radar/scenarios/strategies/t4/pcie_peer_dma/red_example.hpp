#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::pcie_peer_dma {
struct RedResult { bool achieved; int steps; int transactions; bool iommu_bypassed; std::string detail; };
RedResult apply(sim::World& w);
}
