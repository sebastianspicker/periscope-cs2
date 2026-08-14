#pragma once

// Weak T0 evasions novices try — and why they fail against handle-graph blue.

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t0_red {

// Aggregate outcome fields for `WeakEvasionReport` (lab narrative / tests).
struct WeakEvasionReport {
  std::string attempt;
  std::string blue_still_sees;
  bool defeats_handle_graph = false;
  bool applied = false;
  std::string detail;
};

// Lab type `WeakEvasionKit` used by this educational unit.
class WeakEvasionKit {
 public:
  /// Packing / unique build: changes file hash / process name only.
  static WeakEvasionReport polymorphic_rename(sim::World& w, std::uint32_t pid,
                                              const std::string& new_name);

  /// Hide from weak process name lists (not from handles).
  static WeakEvasionReport hide_from_weak_enum(sim::World& w, std::uint32_t pid);

  /// Throttle reads: log volume heuristic; handle still open.
  static WeakEvasionReport throttle_reads(sim::World& w, std::uint32_t pid,
                                          int hz = 20);

  /// Mark handle hidden during single AC enum sample (continuous still sees).
  static WeakEvasionReport hide_handle_during_enum(sim::World& w,
                                                   std::uint32_t owner_pid,
                                                   std::uint32_t game_pid);

  /// Claim looks_reputable (allowlist bait) — blue strong policy still fires.
  static WeakEvasionReport claim_reputation(sim::World& w, std::uint32_t pid);

  /// Run several weak evasions; returns per-step reports.
  static std::vector<WeakEvasionReport> apply_common_stack(
      sim::World& w, std::uint32_t pid, std::uint32_t game_pid);

  /// Multi-step weak path: common stack + offset-C2 / CDN net scar.
  /// Does not defeat handle graph; plants net residual for multi-reason blue.
  static std::vector<WeakEvasionReport> apply_with_net_scar(
      sim::World& w, std::uint32_t pid, std::uint32_t game_pid);
};

}  // namespace t0_red
