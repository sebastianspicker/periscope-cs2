#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Blue executes the simulated Message 159 diagnostic collection pipeline.
bool diagnostic_telemetry_blue_detect(sim::World& w, sim::Narrator& n);
