#pragma once

// Strategy lab framework: every catalog entry is a red/blue pair you can run.
//
// Dual-mode architecture (educational):
//   SIM MODE:  Strategies run against sim::World — deterministic, no game required.
//              All catalog pairs work this way. The classic educational path.
//   REAL MODE: Strategies execute against a real CS2 process via OS/hardware APIs.
//              The game must be running. Shows how the technique works in practice.
//   HYBRID:    Try real CS2 first; if unavailable, fall back to sim.
//              Best of both worlds: real data when possible, sim always works.
//
// Every pair retains its sim::World path. REAL mode adapts the same lesson to
// actual process memory access. Both modes produce the same educational output:
//   - Did red achieve an information advantage?
//   - Did blue detect the technique?
//   - Was the technique mitigated?
//
// Support library (this folder):
//   strategy_types.hpp  — RedOutcome / BlueOutcome / SignalHit
//   scar_sensors.hpp    — World scar inventory (handles, drivers, trust, …)
//   multi_reason.hpp    — Weighted multi-reason blue detector builder
//   pair_util.hpp       — Pair result assembly + handle/entity helpers
//   catalog_util.hpp    — Catalog filter / histogram / validate
//   strategy_support.hpp — Back-compat examples::support helpers

#include "sim/narrative.hpp"
#include "sim/world.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace strategies {

enum class Family {
  Delivery,      // how red gets data/power
  Feature,       // what product feature
  Evasion,       // how red hides
  Detection,     // blue sensor (may also appear as pair blue side)
  Structural,    // server/design/policy
};

// Map Family to display name for filters/list ("Delivery"|"Feature"|...).
const char* family_name(Family f);

// Case-insensitive C-string compare (used by run_filtered).
bool iequals(const char* a, const char* b);

// Lab type `StrategyMeta` used by this educational unit.
struct StrategyMeta {
  const char* id;          // e.g. "01_external_rpm"
  const char* title;
  Family family;
  const char* tiers;       // "T0" / "T0-T1" / "all"
  const char* red_one_liner;
  const char* blue_one_liner;
};

// Aggregate outcome fields for `StrategyResult` (lab narrative / tests).
struct StrategyResult {
  bool red_achieved = false;   // red got the advantage this step
  bool blue_detected = false;  // blue caught the scar
  bool blue_mitigated = false; // structural kill / policy deny
  std::string summary;
};

using StrategyFn = StrategyResult (*)(sim::World& w, sim::Narrator& n);

// Lab type `StrategyEntry` used by this educational unit.
struct StrategyEntry {
  StrategyMeta meta;
  StrategyFn run;
};

// Lab type `CatalogStats` used by this educational unit.
struct CatalogStats {
  int total = 0;
  int by_family[5] = {};  // index by Family cast
  int red_achieved = 0;
  int blue_detected = 0;
  int blue_mitigated = 0;
  int failures = 0;
  std::string detail;
};

// ── Dual-mode result ──────────────────────────────────────────────

/// Strategy result from REAL mode (operating on real CS2 process).
/// Each field has the same semantics as StrategyResult but reflects
/// what happened on the actual game process rather than sim::World.
struct RealStrategyResult {
  bool red_achieved = false;   ///< Red successfully read entity data
  bool handle_opened = false;  ///< Red opened a handle (scar left)
  int entities_read = 0;       ///< How many entities were read
  bool blue_would_detect = false; ///< Blue would detect this technique
  std::string summary;         ///< Educational narrative
  std::string error_msg;       ///< If real mode failed
};

/// Dual-mode result: contains sim outcome + real outcome.
/// Educational tools display both for comparison.
///
/// Example output:
///   SIM:  red_achieved=true | blue_detected=false  [expected, T0 RPM]
///   REAL: red_achieved=true | handle_opened=true    [scar visible in handle table]
///   LESSON: The handle scar exists in both worlds!
struct DualResult {
  StrategyResult sim;              ///< Outcome on sim::World
  RealStrategyResult real;         ///< Outcome on real CS2 process
  const char* mode_used = "sim";   ///< "real", "sim", or "hybrid"

  /// Human-readable comparison.
  /// Prints both SIM and REAL narratives side by side.
  std::string describe() const;
};

// ── Dual-mode API ─────────────────────────────────────────────────

/// Run a strategy pair in REAL mode against a live CS2 process.
/// Requires cs2.exe to be running and accessible.
/// T0 path: Opens a handle to cs2.exe, reads entity list, closes.
/// T1+ paths: Use progressively deeper OS/hardware access.
/// Returns RealStrategyResult with educational narrative.
RealStrategyResult run_real(const char* strategy_id, bool verbose);

/// Run a strategy pair in dual mode.
/// Tries real CS2 if may_use_real(), then sim always.
/// Returns both results for educational comparison.
DualResult run_dual(const char* strategy_id, bool verbose);

/// Check if a strategy's technique can be applied to a real CS2 process.
/// Most T0/T1 delivery strategies work on real processes.
/// T2+ strategies require kernel/HW access beyond the default safety level.
bool supports_real_mode(const StrategyMeta& meta);

// ── Sim-only API (unchanged) ──────────────────────────────────────

// Full registered catalog (static StrategyEntry factories).
const std::vector<StrategyEntry>& catalog();
// Lookup by meta.id; nullptr if unknown.
const StrategyEntry* find(std::string_view id);
// Run every catalog pair; 0 only if all pass (blue win or red fail).
int run_all(bool verbose);
// Run one pair on a fresh World; 0 if blue detects/mitigates or !red_achieved.
int run_one(std::string_view id, bool verbose);
// family_filter nullable (case-insensitive name), tier_filter nullable (substr on meta.tiers)
int run_filtered(const char* family_filter, const char* tier_filter, bool verbose,
                 CatalogStats* out_stats = nullptr);

}  // namespace strategies
