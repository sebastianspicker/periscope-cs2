#pragma once
// Lab RED: anti-debug / anti-suspend / destruct protector residual.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::protector_suite {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::protector_suite
