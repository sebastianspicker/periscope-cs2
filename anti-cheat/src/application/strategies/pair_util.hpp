// pair_util.hpp — helpers shared by strategy pair.cpp runners (narrative glue).

#pragma once

#include "strategies/framework.hpp"
#include "strategies/strategy_types.hpp"

#include "sim/narrative.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace strategies {

/// Resolve the educational game process id (0 if none).
std::uint32_t find_game(sim::World& w);

/// True if any foreign (non-game, non-AC) process holds VM_READ on `game`.
bool any_vm_read(const sim::World& w, std::uint32_t game);

/// Read the 4-byte entity count at game base via an open reader handle.
/// Returns -1 on failure.
int entity_count_via_handle(sim::World& w, std::uint32_t reader,
                            std::uint32_t game);

/// Count foreign VM_READ edges including hidden/brief handles.
int count_foreign_vm_read_owners(const sim::World& w, std::uint32_t game);

/// OpenProcess(VM_READ) then read entity count; returns true on success.
bool open_and_read_entity_count(sim::World& w, std::uint32_t reader,
                                std::uint32_t game, int* out_count);

/// Build a StrategyResult from primitive flags + summary.
StrategyResult make_pair_result(bool red_achieved, bool blue_detected,
                                bool blue_mitigated, std::string summary);

/// Assemble StrategyResult from canonical red/blue outcomes; optional narrator.
StrategyResult assemble_pair_result(const RedOutcome& red,
                                    const BlueOutcome& blue,
                                    sim::Narrator* n = nullptr);

/// Lab pass criterion: blue detects/mitigates or red fails to achieve.
bool pair_pass(const StrategyResult& r);

/// One-line describe for logs/tests.
std::string describe_pair_result(const StrategyResult& r);

}  // namespace strategies
