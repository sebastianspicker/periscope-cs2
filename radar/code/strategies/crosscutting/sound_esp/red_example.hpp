#pragma once
// Lab RED: infer enemy positions from audio without line-of-sight.
#include "sim/world.hpp"
#include <string>
namespace examples::sound_esp {
struct RedResult {
  bool achieved = false;
  int sound_sources = 0;
  int entities_visible = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::sound_esp
