#pragma once

// Lab RED example for strategy `dse_testsign` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::dse_testsign {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `dse_testsign`.
RedResult apply(sim::World& w);

}  // namespace examples::dse_testsign
