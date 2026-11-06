#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Red records a thread start address inside an expected loaded module.
bool thread_monitor_red_apply(sim::World& w, sim::Narrator& n);
