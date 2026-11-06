#pragma once

// T1 counter: reuses handle graph; explicitly ignores "API hook saw nothing".
// Full multi-step agent combining hook blindness proof + handle truth + staging.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t1_blue/staging_detector.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_blue {

// Aggregate outcome fields for `T1Detection` (lab narrative / tests).
struct T1Detection {
  bool handle_truth_hit = false;
  bool hooks_blind = false;
  bool staging_hit = false;
  bool etw_blind_hit = false;
  bool stack_spoof_hit = false;
  bool hollow_hit = false;
  bool parent_lineage_hit = false;
  int foreign_vm_read = 0;
  int syscall_handles = 0;
  int hook_visible = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string summary;
};

// Lab type `SyscallAwareHandleMonitor` used by this educational unit.
class SyscallAwareHandleMonitor {
 public:
  explicit SyscallAwareHandleMonitor(t0_blue::HandleGraphMonitor& handles);

  /// Edge-list evaluate (proto demos).
  void evaluate(std::uint32_t game_pid,
                const std::vector<t0_blue::HandleEdge>& edges,
                bool usermode_hooks_saw_rpm);

  bool detection_depends_on_ntdll_hooks() const { return false; }
  std::size_t last_hit_count() const { return last_hits_; }

 private:
  t0_blue::HandleGraphMonitor& handles_;
  std::size_t last_hits_ = 0;
};

/// Full T1 AC agent on sim::World.
class T1Agent {
 public:
  explicit T1Agent(sim::World& world, ac::ITelemetrySink& sink);

  // Full multi-sensor blue scan: merge independent reasons/risk (not one bool).
  T1Detection full_scan();
  const ac::RiskState& risk() const { return risk_.state(); }

 private:
  sim::World& world_;
  ac::ITelemetrySink& sink_;
  ac::RiskAggregator risk_;
  t0_blue::HandleGraphMonitor handles_;
  SyscallAwareHandleMonitor mon_;
  UsermodeHookTrap hooks_;
  HandleTruthMonitor truth_;
  StagingDetector staging_;
};

}  // namespace t1_blue
