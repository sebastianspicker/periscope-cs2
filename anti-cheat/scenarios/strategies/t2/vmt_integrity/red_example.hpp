#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Red changes one simulated interface VMT entry to a foreign target.
bool vmt_integrity_red_apply(sim::World& w, sim::Narrator& n);
