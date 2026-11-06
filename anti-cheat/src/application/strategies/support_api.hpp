#pragma once

// Non-inline support API surface (link against ac_strategy_core).

#include "sim/world.hpp"

namespace strategies::support_detail {

/// ABI / schema version of the strategies support library.
int library_abi_version();

/// Score overall World risk via full scar inventory (0..1).
double score_world_risk(const sim::World& w);

/// Count total scars via inventory().total_scars.
int count_world_scars(const sim::World& w);

}  // namespace strategies::support_detail
