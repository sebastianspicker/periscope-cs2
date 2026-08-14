#pragma once

// Secondary T0 signal: suspicious processes living next to the game session.
//
// Enhanced with real process scanning via CreateToolhelp32Snapshot,
// detection of known cheat process patterns.

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/api_table.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace t0_blue {

// Lab type `ProcessRecord` used by this educational unit.
struct ProcessRecord {
  std::uint32_t pid = 0;
  std::string name;
};

// Lab type `CooccurrenceHit` used by this educational unit.
struct CooccurrenceHit {
  std::uint32_t pid = 0;
  std::string name;
  std::string reason;
  double risk_delta = 1.0;
};

// Aggregate outcome fields for `CooccurrenceResult` (lab narrative / tests).
struct CooccurrenceResult {
  bool hit = false;
  std::vector<CooccurrenceHit> hits;
  std::string detail;
};

// Lab type `ProcessCooccurrence` used by this educational unit.
class ProcessCooccurrence {
 public:
  explicit ProcessCooccurrence(ac::ITelemetrySink& sink);

  /// Feed a process list for the session (proto demos).
  void on_game_session(std::uint32_t game_pid,
                       const std::vector<ProcessRecord>& live);

  /// Scan sim::World process table (primary path).
  CooccurrenceResult scan_world(const sim::World& w, std::uint32_t game_pid,
                                bool weak_enum = true);

  const CooccurrenceResult& last() const { return last_; }

  // ── Real process scanning ───────────────────────────────────

  /// Scan real processes via CreateToolhelp32Snapshot (API table).
  CooccurrenceResult scan_real_processes(uint32_t game_pid);

  /// Known cheat process name patterns (lowercase).
  static bool matches_known_cheat_pattern(const std::string& name);

 private:
  static bool looks_suspicious_name(const std::string& name);

  ac::ITelemetrySink& sink_;
  CooccurrenceResult last_{};
};

}  // namespace t0_blue
