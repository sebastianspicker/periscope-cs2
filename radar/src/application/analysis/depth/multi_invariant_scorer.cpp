// multi_invariant_scorer.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/multi_invariant_scorer.hpp"

#include <algorithm>
#include <sstream>

namespace depth {

// MultiInvariantScorer::on_frame: Score one frame under multi-invariant rules.
void MultiInvariantScorer::on_frame(const RichDemoFrame& f) {
  ++frames_;
  bool bad = false;

  // Vision invariant: aim on target with no vision.
  if (f.aim_on_hidden_target && !f.has_vision_on_target) {
    ++vision_hits_;
    raw_ += 1.0;
    bad = true;
  }

  // Sound invariant: aim without audio cue either (stronger when no vision).
  if (f.aim_on_hidden_target && !f.has_vision_on_target &&
      !f.has_audio_on_target) {
    ++sound_hits_;
    raw_ += 0.75;
    bad = true;
  }

  // Latency invariant: pre-aim reaction faster than RTT budget (lab: < rtt/4).
  // Model: if aiming at hidden target with very low effective reaction vs RTT.
  if (f.aim_on_hidden_target && !f.has_vision_on_target && f.rtt_ms > 40.0) {
    // High RTT makes "perfect" pre-aim more suspicious.
    ++latency_hits_;
    raw_ += 0.5 + (f.rtt_ms / 200.0);
    bad = true;
  }

  // Report graph invariant.
  if (f.peer_reports_on_actor >= 2 || f.report_filed_this_window) {
    ++report_hits_;
    raw_ += 0.5 * static_cast<double>(std::max(1, f.peer_reports_on_actor));
    bad = true;
  }

  if (bad) {
    ++consecutive_bad_;
    peak_consecutive_ = std::max(peak_consecutive_, consecutive_bad_);
  } else {
    consecutive_bad_ = 0;
  }
}

// MultiInvariantScorer::on_classic: Classic multi-invariant score on a frame bundle.
void MultiInvariantScorer::on_classic(const server::DemoFrame& f) {
  RichDemoFrame r;
  r.t = f.t;
  r.eye = f.eye;
  r.aim_yaw = f.aim_yaw;
  r.has_vision_on_target = f.has_vision_on_target;
  r.has_audio_on_target = f.has_audio_on_target;
  r.aim_on_hidden_target = f.aim_on_hidden_target;
  r.rtt_ms = 50.0;
  on_frame(r);
}

// MultiInvariantScorer::result: Print BLUE/RED outcome banner + detail.
MultiInvariantResult MultiInvariantScorer::result() const {
  MultiInvariantResult r;
  r.vision_hits = vision_hits_;
  r.sound_hits = sound_hits_;
  r.latency_hits = latency_hits_;
  r.report_hits = report_hits_;
  r.raw_score = raw_;
  r.frames = frames_;
  r.fp_budget_remaining = std::max(0.0, fp_budget_ - raw_);
  r.adjusted_score = std::max(0.0, raw_ - fp_budget_);

  // Delayed thresholds: need consecutive bad frames OR high adjusted score.
  const bool sustained = peak_consecutive_ >= delay_frames_;
  const double s = r.adjusted_score;

  if (s >= ban_th_ && sustained) {
    r.action = DelayedAction::DelayedBanCandidate;
    r.threshold_crossed = true;
  } else if (s >= restrict_th_ && sustained) {
    r.action = DelayedAction::SoftRestrict;
    r.threshold_crossed = true;
  } else if (s >= overwatch_th_) {
    r.action = DelayedAction::FlagOverwatch;
    r.threshold_crossed = true;
  } else if (s >= observe_th_ || (vision_hits_ + sound_hits_) >= 2) {
    r.action = DelayedAction::ObserveOnly;
  } else {
    r.action = DelayedAction::None;
  }

  // Independent invariant count for pedagogy checks.
  int invariants = 0;
  if (vision_hits_ > 0) {
    ++invariants;
  }
  if (sound_hits_ > 0) {
    ++invariants;
  }
  if (latency_hits_ > 0) {
    ++invariants;
  }
  if (report_hits_ > 0) {
    ++invariants;
  }

  std::ostringstream oss;
  oss << "vis=" << vision_hits_ << " snd=" << sound_hits_
      << " lat=" << latency_hits_ << " rep=" << report_hits_
      << " raw=" << raw_ << " adj=" << r.adjusted_score
      << " fp_left=" << r.fp_budget_remaining
      << " invariants=" << invariants
      << " consec=" << peak_consecutive_
      << " action=" << static_cast<int>(r.action);
  r.detail = oss.str();
  return r;
}

// MultiInvariantScorer::reset: Clear aggregate score/state for a new lab scenario.
void MultiInvariantScorer::reset() {
  vision_hits_ = sound_hits_ = latency_hits_ = report_hits_ = 0;
  raw_ = 0;
  frames_ = consecutive_bad_ = peak_consecutive_ = 0;
}

MultiInvariantResult apply_overwatch_to_world(sim::World& w,
                                              MultiInvariantScorer& scorer) {
  auto r = scorer.result();
  w.overwatch_score = r.adjusted_score;
  // Multi-step delayed action path: observe → overwatch queue → restrict → ban.
  switch (r.action) {
    case DelayedAction::DelayedBanCandidate:
      w.overwatch_queued = true;
      w.ranked_access_denied = true;
      break;
    case DelayedAction::SoftRestrict:
      w.overwatch_queued = true;
      // Soft path: still allow session but flag for ops review.
      break;
    case DelayedAction::FlagOverwatch:
      w.overwatch_queued = true;
      break;
    case DelayedAction::ObserveOnly:
    case DelayedAction::None:
      break;
  }
  w.note("multi_invariant " + r.detail +
         " action=" + std::to_string(static_cast<int>(r.action)));
  return r;
}

}  // namespace depth
