#pragma once

#include "sim/narrative.hpp"
#include "sim/world.hpp"

// MODEL ONLY: builds simulated proxy-dispatch telemetry; it never changes host VMTs.
bool vmt_proxy_evade_red_apply(sim::World& w, sim::Narrator& n);
