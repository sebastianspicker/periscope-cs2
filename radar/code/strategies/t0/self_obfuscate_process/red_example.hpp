#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

// MODEL ONLY: records process-identity telemetry in sim::World; it makes no OS changes.
bool self_obfuscate_process_red_apply(sim::World& w, sim::Narrator& n);
