// info_advantage.cpp — server info-advantage scorer (aim vs vision/audio truth).
// Multi-invariant behavior score; pairs with structural fog.

#include "server/info_advantage.hpp"

namespace server {
namespace {

// Rebuild independent reason tags and multi_reason from accumulators.
void refresh_reasons(InfoAdvantageScore& r) {
  r.reasons.clear();

  // Independent support A: volume of pre-aim hits.
  if (r.hits >= 3) {
    r.reasons.push_back("pre_aim_volume");
  }
  // Independent support B: consecutive streak without a clean break.
  if (r.max_consecutive >= 3) {
    r.reasons.push_back("pre_aim_streak");
  }
  // Independent support C: multi-frame sample window with enough hits.
  if (r.frames_seen >= 4 && r.hits >= 3) {
    r.reasons.push_back("multi_frame_sample");
  }

  // Multi-reason: two or more independent supports, or volume residual alone
  // at the lab threshold (hits >= 3) which is the shared_full_tests gate.
  r.multi_reason = r.reasons.size() >= 2 || r.hits >= 3;
}

}  // namespace

// InfoAdvantageScorer::on_frame: Score one aim frame vs vision/audio truth invariants.
void InfoAdvantageScorer::on_frame(const DemoFrame& f) {
  ++result_.frames_seen;

  // Pre-aim residual: aim on hidden target with neither vision nor audio.
  const bool hit = f.aim_on_hidden_target && !f.has_vision_on_target &&
                   !f.has_audio_on_target;
  if (hit) {
    result_.hits += 1;
    result_.score += 1.0;
    ++streak_;
    if (streak_ > result_.max_consecutive) {
      result_.max_consecutive = streak_;
    }
  } else {
    // Clean / explained frame: break consecutive streak only.
    streak_ = 0;
  }

  refresh_reasons(result_);
}

}  // namespace server
