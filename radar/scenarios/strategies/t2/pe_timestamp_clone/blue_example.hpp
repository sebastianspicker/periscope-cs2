#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Blue verifies module PE timestamps and cross-checks module identity
/// so a cloned timestamp is not enough to hide a foreign module.
bool pe_timestamp_clone_blue_detect(sim::World& w, sim::Narrator& n);
