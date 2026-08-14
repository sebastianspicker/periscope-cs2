#pragma once

// Multi-sample handle composition for T0/T1 pedagogy.
// Detects open/close races, brief-reopen, reputation allowlist, and parent lineage.
// Not a single World bool read.

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace depth {

// Lab type `HandleSample` used by this educational unit.
struct HandleSample {
  int tick = 0;
  int foreign_vm_read_visible = 0;   // enum without hidden edges
  int foreign_vm_read_truth = 0;     // ground truth including hidden
  int brief_reopen_edges = 0;
  int hidden_edges = 0;
  std::vector<std::uint32_t> visible_owner_pids;
  std::vector<std::uint32_t> truth_owner_pids;
};

// Lab type `LineageHit` used by this educational unit.
struct LineageHit {
  std::uint32_t reader_pid = 0;
  std::uint32_t parent_pid = 0;
  std::string parent_name;
  bool parent_suspicious = false;
};

// Lab type `ReputationHit` used by this educational unit.
struct ReputationHit {
  std::uint32_t pid = 0;
  std::string name;
  bool looks_reputable = false;
  bool allowlisted = false;  // blue policy would suppress solo FP
};

// Aggregate outcome fields for `MultiSampleHandleResult` (lab narrative / tests).
struct MultiSampleHandleResult {
  bool race_detected = false;       // samples disagree (hide/reopen race)
  bool continuous_hit = false;      // truth ever saw foreign VM_READ
  bool lineage_hit = false;
  bool reputation_evasion = false;  // reputable-looking + read edge
  bool composed_hit = false;        // multi-invariant composition
  double score = 0;
  int samples_taken = 0;
  int max_visible = 0;
  int max_truth = 0;
  std::vector<LineageHit> lineage;
  std::vector<ReputationHit> reputation;
  std::string detail;
};

/// Blue multi-sample detector: take ≥2 AC enum samples while red may hide/reopen.
class MultiSampleHandleDetector {
 public:
  /// Capture one AC sample at current world handle state.
  HandleSample sample(const sim::World& w, int tick) const;

  /// Record a sample into the detector history.
  void push(const HandleSample& s);

  /// After ≥2 samples, compose race + lineage + reputation signals.
  MultiSampleHandleResult evaluate(const sim::World& w) const;

  void reset() { samples_.clear(); }
  const std::vector<HandleSample>& samples() const { return samples_; }

 private:
  std::vector<HandleSample> samples_;
};

/// Red multi-step race helper: open → hide during sample → optional brief reopen.
struct HandleRaceScript {
  std::uint32_t actor_pid = 0;
  bool opened = false;
  bool hid = false;
  bool reopened = false;
  bool read_ok = false;
  std::string detail;
};

/// Plant red open/hide/reopen race against multi-sample AC on sim::World.
HandleRaceScript run_handle_race(sim::World& w, int samples = 3,
                                 bool use_syscall = false,
                                 bool claim_reputable = false,
                                 std::uint32_t parent_pid = 0);

}  // namespace depth
