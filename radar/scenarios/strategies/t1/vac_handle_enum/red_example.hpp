#pragma once

#include "ac/types.hpp"
#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: A non-Steam process opens a VM_READ edge to the game in sim::World.
ac::Status vac_handle_red_apply(sim::World& w, sim::Narrator& n);
