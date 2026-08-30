#pragma once
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::sound_viz_subtypes {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::sound_viz_subtypes
