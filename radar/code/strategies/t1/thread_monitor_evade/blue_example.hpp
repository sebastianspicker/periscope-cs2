#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Blue evaluates the captured Win32 thread start address in simulation.
bool thread_monitor_blue_detect(sim::World& w, sim::Narrator& n);
