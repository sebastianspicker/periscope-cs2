#pragma once
// Composition RED orchestrator: scan→refresh→entity product.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::composition_radar_loop {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  bool scanned = false;
  bool refreshed = false;
  int entities = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::composition_radar_loop
