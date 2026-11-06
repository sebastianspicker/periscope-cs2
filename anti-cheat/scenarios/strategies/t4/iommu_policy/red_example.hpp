#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::iommu_policy {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::iommu_policy
