#pragma once

// Lab RED for `fallback_chain` on sim::World only (theme-specific multi-step scars).

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::fallback_chain {

struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  bool hv_failed = false;
  bool kernel_ok = false;
  bool rpm_ok = false;
};

RedResult apply(sim::World& w);

}  // namespace examples::fallback_chain
