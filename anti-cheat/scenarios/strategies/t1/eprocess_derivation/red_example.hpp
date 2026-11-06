#pragma once

// Red simulation for eprocess_derivation. It derives structural offsets at
// runtime from self-validating invariants (list cycle, DTB probe, PEB Ldr
// cycle) and re-derives after layout churn - no shipped offsets, no scanner.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::eprocess_derivation {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::eprocess_derivation
