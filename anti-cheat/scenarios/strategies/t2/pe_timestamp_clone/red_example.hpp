#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Red clones a legitimate module's PE timestamp onto a manual-mapped
/// cheat module so the simulated PETimestampSensor sees an allowlisted value.
bool pe_timestamp_clone_red_apply(sim::World& w, sim::Narrator& n);
