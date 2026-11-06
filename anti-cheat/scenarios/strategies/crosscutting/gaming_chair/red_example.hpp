#pragma once
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::gaming_chair {
struct RedConfig {
  bool proxy_hijack = true;
  bool band4 = true;
  bool esp = true;
  bool aimbot = false;
};
struct RedResult {
  bool achieved = false;
  int steps = 0;
  int active = 0;
  std::uint32_t actor_pid = 0;
  std::vector<std::string> features;
  std::string detail;
};
RedResult apply(sim::World& w, RedConfig cfg = {});
}  // namespace examples::gaming_chair
