#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

#include <string>

namespace examples::convars_temp_restore {

struct Result {
  bool achieved = false;
  bool restored = false;
  std::string detail;
};

Result apply(sim::World& w, sim::Narrator& n);

}  // namespace examples::convars_temp_restore
