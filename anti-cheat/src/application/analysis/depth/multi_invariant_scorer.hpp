#pragma once

// Multi-invariant demo scoring: vision + sound + latency + report graph.
// Delayed action thresholds and FP budget — shipped scorer, not test stubs.

#include "server/info_advantage.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace depth {

// Lab type `RichDemoFrame` used by this educational unit.
struct RichDemoFrame {
  double t = 0;
  ac::Vec3 eye{};
  float aim_yaw = 0;
  bool has_vision_on_target = false;
  bool has_audio_on_target = false;
  bool aim_on_hidden_target = false;
  double rtt_ms = 30.0;               // latency invariant
  bool report_filed_this_window = false;
  int peer_reports_on_actor = 0;      // report graph
  bool input_source_raw = true;
};

enum class DelayedAction : std::uint8_t {
  None = 0,
  ObserveOnly,
  FlagOverwatch,
  SoftRestrict,
  DelayedBanCandidate,
};

// Aggregate outcome fields for `MultiInvariantResult` (lab narrative / tests).
struct MultiInvariantResult {
  int vision_hits = 0;     // aim without vision
  int sound_hits = 0;      // aim without sound (and no vision)
  int latency_hits = 0;    // impossible pre-aim vs RTT
  int report_hits = 0;     // correlated reports
  double raw_score = 0;
  double adjusted_score = 0;  // after FP budget
  double fp_budget_remaining = 0;
  DelayedAction action = DelayedAction::None;
  int frames = 0;
  bool threshold_crossed = false;
  std::string detail;
};

/// Multi-invariant info-advantage / overwatch scorer with delayed thresholds.
class MultiInvariantScorer {
 public:
  void set_fp_budget(double b) { fp_budget_ = b; }
  void set_observe_threshold(double t) { observe_th_ = t; }
  void set_overwatch_threshold(double t) { overwatch_th_ = t; }
  void set_restrict_threshold(double t) { restrict_th_ = t; }
  void set_ban_threshold(double t) { ban_th_ = t; }
  void set_delay_frames(int n) { delay_frames_ = n; }

  // Score one rich demo frame (aim vs vision/audio) under multi-invariant rules.
  void on_frame(const RichDemoFrame& f);
  MultiInvariantResult result() const;
  void reset();

  /// Convert classic DemoFrame path into rich frames (vision/sound only).
  void on_classic(const server::DemoFrame& f);

 private:
  double fp_budget_ = 2.0;  // absorb weak noise before action
  double observe_th_ = 1.5;
  double overwatch_th_ = 3.0;
  double restrict_th_ = 6.0;
  double ban_th_ = 10.0;
  int delay_frames_ = 3;  // need sustained hits before escalate

  int vision_hits_ = 0;
  int sound_hits_ = 0;
  int latency_hits_ = 0;
  int report_hits_ = 0;
  double raw_ = 0;
  int frames_ = 0;
  int consecutive_bad_ = 0;
  int peak_consecutive_ = 0;
};

/// Apply scorer to World overwatch fields (queued score / delayed path).
MultiInvariantResult apply_overwatch_to_world(sim::World& w,
                                              MultiInvariantScorer& scorer);

}  // namespace depth
