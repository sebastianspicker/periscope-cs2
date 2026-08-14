#pragma once

// Lab RED example for strategy `kernel_ioctl` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <string>

namespace examples::kernel_ioctl {

struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

RedResult apply(sim::World& w);
RedResult run_red(sim::World& w, sim::Narrator& n);

}  // namespace examples::kernel_ioctl
