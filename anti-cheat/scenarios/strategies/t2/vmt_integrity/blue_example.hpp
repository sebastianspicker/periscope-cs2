#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

/// MODEL: Blue runs the CS2 interface/entity VMT integrity sensors.
bool vmt_integrity_blue_detect(sim::World& w, sim::Narrator& n);
