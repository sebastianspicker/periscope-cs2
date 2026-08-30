#pragma once

// Info-advantage behavioral residual scorer for the educational lab.
// Detects pre-aim / track without vision or sound across demo frames.
// Simulated educational residual — not a production overwatch model.

#include "ac/types.hpp"

#include <string>
#include <vector>

namespace server {

// One demo/match frame of aim vs vision/audio truth for residual scoring.
struct DemoFrame {
  double t = 0;
  ac::Vec3 eye{};
  float aim_yaw = 0;
  bool has_vision_on_target = false;
  bool has_audio_on_target = false;
  bool aim_on_hidden_target = false;
};

// Accumulated pre-aim residual score with multi-reason supports.
struct InfoAdvantageScore {
  double score = 0;
  int hits = 0;
  int max_consecutive = 0;
  int frames_seen = 0;
  bool multi_reason = false;
  std::vector<std::string> reasons;
};

/// Behavioral residual counter for all tiers (including "clean" clients).
///
/// A frame is a pre-aim hit when aim is on a hidden target and neither vision
/// nor audio explains that aim. Hits accumulate score/hits/streak; clean frames
/// reset the consecutive streak. Independent reason supports fire once
/// thresholds are met:
/// - pre_aim_volume: hits >= 3
/// - pre_aim_streak: max_consecutive >= 3
/// - multi_frame_sample: frames_seen >= 4 and hits >= 3
/// multi_reason is true when at least two independent supports fire, or when
/// volume alone reaches the multi-hit residual used by shared tests.
class InfoAdvantageScorer {
 public:
  // Ingest one DemoFrame; update streak/score for unfair pre-aim knowledge.
  void on_frame(const DemoFrame& f);
  InfoAdvantageScore result() const { return result_; }
  void reset() {
    result_ = {};
    streak_ = 0;
  }

 private:
  InfoAdvantageScore result_{};
  int streak_ = 0;
};

}  // namespace server
