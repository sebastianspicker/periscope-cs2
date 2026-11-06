#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::skin_changer {
struct RedResult { bool achieved = false; int modified = 0; std::string detail; };
RedResult apply(sim::World& w);
}
