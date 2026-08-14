#pragma once
// Lab RED: triggerbot fire-latency residual.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::triggerbot_timing {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int fires = 0;
  float mean_latency_ms = 0.f;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::triggerbot_timing
