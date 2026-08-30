#pragma once

// Lab RED for `interest_mgmt` on sim::World only (theme-specific multi-step scars).

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::interest_mgmt {

struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
  bool wants_full_origin = false;
  bool key_exfil = false;
};

RedResult apply(sim::World& w);

}  // namespace examples::interest_mgmt
