#pragma once

// Red simulation for handle_origin_chain (165). A donor opens the game, a
// relay duplicates from the donor, and the consumer duplicates from the relay.
// The consumer's handle names the relay as owner, so naive origin attribution
// stops one hop short of the donor. Sim only.
#include "sim/world.hpp"

#include <string>

namespace examples::handle_origin_chain {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::handle_origin_chain
