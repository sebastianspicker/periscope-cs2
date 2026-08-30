#pragma once

// Primary T0 counter: who holds read access to the game process?
// Full multi-sample + World scan path (not edge-list-only demos).
//
// Enhanced with real handle scanning via NtQuerySystemInformation.
// Filters handles pointing to CS2 PID, detects VM_READ access,
// checks owning PID legitimacy.

#include "ac/telemetry.hpp"
#include "sim/world.hpp"


#include <cstdint>
#include <string>
#include <vector>

namespace t0_blue {

enum class ProcessAccess : std::uint32_t {
  None = 0,
  QueryLimited = 1u << 0,
  VmRead = 1u << 1,
};

// Lab type `HandleEdge` used by this educational unit.
struct HandleEdge {
  std::uint32_t source_pid = 0;
  std::uint32_t target_pid = 0;
  ProcessAccess access = ProcessAccess::None;
  std::string source_name;
  bool via_syscall = false;
  bool hidden_during_enum = false;
  bool brief_reopen = false;
};

// Aggregate outcome fields for `HandleSampleResult` (lab narrative / tests).
struct HandleSampleResult {
  int visible_foreign_vm_read = 0;
  int truth_foreign_vm_read = 0;
  int hidden_edges = 0;
  int brief_reopen = 0;
  bool race_suspected = false;  // visible < truth
  std::vector<HandleEdge> suspicious;
  std::string detail;
};

// Lab type `HandleGraphMonitor` used by this educational unit.
class HandleGraphMonitor {
 public:
  explicit HandleGraphMonitor(ac::ITelemetrySink& sink);

  /// Feed pre-built edges (proto demos / unit injection).
  void ingest_edges(std::uint32_t game_pid, const std::vector<HandleEdge>& edges);

  /// Scan sim::World handle table (primary educational path).
  HandleSampleResult scan_world(const sim::World& w, std::uint32_t game_pid,
                                bool include_hidden = true);

  /// Multi-sample: call scan_world twice with include_hidden false/true.
  HandleSampleResult multi_sample(const sim::World& w, std::uint32_t game_pid);

  const std::vector<HandleEdge>& suspicious() const { return suspicious_; }
  const HandleSampleResult& last() const { return last_; }

  // ── Real handle scanning ────────────────────────────────────

  /// Real-time handle scan via NtQuerySystemInformation(SystemExtendedHandleInformation).
  /// Filters handles targeting cs2_pid with PROCESS_VM_READ access.
  HandleSampleResult scan_real_handles(uint32_t cs2_pid);

  /// Check if owning PID is legitimate (system process, known tool, etc.).
  bool is_pid_legitimate(uint32_t owner_pid) const;

  /// Get total handles observed in last real scan.
  int total_handles_scanned() const { return total_handles_scanned_; }

 private:
  ac::ITelemetrySink& sink_;
  std::vector<HandleEdge> suspicious_;
  HandleSampleResult last_{};
  int total_handles_scanned_ = 0;
};

}  // namespace t0_blue
