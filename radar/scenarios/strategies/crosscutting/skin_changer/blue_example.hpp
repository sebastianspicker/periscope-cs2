#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::skin_changer {
struct BlueResult { bool detected = false; int violations = 0; std::vector<std::string> attrs; std::string detail; };
BlueResult detect(sim::World& w);
}
