#pragma once

// T0 AC agent: full multi-sensor scan on sim::World.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t0_blue/false_positive_policy.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t0_blue/process_cooccurrence.hpp"

#include <string>
#include <vector>

namespace t0_blue {

// Aggregate outcome fields for `Detection` (lab narrative / tests).
struct Detection {
  bool handle_hit = false;
  bool cooccurrence_hit = false;
  bool inject_hit = false;
  bool module_integrity_hit = false;
  bool overlay_hit = false;
  bool race_hit = false;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string summary;
};

// Multi-sensor educational blue agent `AcAgent` — reasons/risk on World scars.
class AcAgent {
 public:
  explicit AcAgent(sim::World& world, ac::ITelemetrySink& sink);

  /// Primary weapon: who holds VM_READ on the game?
  Detection scan_handles();

  /// Secondary: suspicious names / unnamed readers co-running.
  Detection scan_cooccurrence();

  /// Inject / foreign thread / manual map scars on game process.
  Detection scan_injection();

  /// Game module .text / hook scars.
  Detection scan_module_integrity();

  /// External topmost overlay windows.
  Detection scan_overlays();

  /// Multi-sample handle race (hide-on-enum).
  Detection scan_handle_race();

  /// Full stack: all sensors + combined reasons/risk.
  Detection full_scan();

  const ac::RiskState& risk() const { return risk_.state(); }
  std::uint32_t game_pid() const { return game_pid_; }

  FalsePositivePolicy& fp_policy() { return fp_; }

 private:
  void merge_(Detection& into, const Detection& part);

  sim::World& world_;
  ac::ITelemetrySink& sink_;
  ac::RiskAggregator risk_;
  FalsePositivePolicy fp_;
  HandleGraphMonitor handles_;
  ProcessCooccurrence co_;
  std::uint32_t game_pid_ = 0;
  std::uint32_t self_pid_ = 0;
};

}  // namespace t0_blue
