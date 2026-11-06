#pragma once

// Blue demonstrates the blind spot of handle-only attribution in the simulation.
#include "sim/world.hpp"
#include "sim/narrative.hpp"

namespace examples::proxy_hijack_reader {

bool detect(sim::World& w, sim::Narrator& n);

}  // namespace examples::proxy_hijack_reader
