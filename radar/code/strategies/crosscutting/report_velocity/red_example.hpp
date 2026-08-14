#pragma once
// Lab RED for `report_velocity` (multi-step report-farm cluster).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::report_velocity {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int report_sum = 0;
  int steps = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::report_velocity
