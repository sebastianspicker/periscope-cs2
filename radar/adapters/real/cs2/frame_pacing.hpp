// frame_pacing.hpp — Pure frame-pacing math for the live radar loop
// (assessment ledger E15).
//
// The loop used to run unthrottled between vsync boundaries; this header
// provides the hybrid sleep+spin schedule popularized by tight external
// loops: sleep until ~1.5 ms before the deadline, then spin the remainder
// with a pause instruction for sub-millisecond precision. Sleeping the whole
// gap wastes up to a timer tick (≈1–15 ms depending on the system timer
// resolution); spinning the whole gap burns a core.
//
// Pure and host-testable: the caller supplies the target period, the current
// steady-clock instant, and the last frame instant; the header returns the
// split. The idle cap bounds the loop rate when the radar has nothing to
// draw (overlay hidden, no entities), trading latency for power/heat.
//
// Platform note: the caller performs the actual sleep (Sleep(ms) /
// waitable timer) and spin (_mm_pause); see live_radar_loop.cpp. The math
// here never depends on timer-resolution side effects.

#pragma once

#include <chrono>
#include <cstdint>

namespace real::cs2::pacing {

/// Target rates.
struct PaceConfig {
  /// Active frame period (0 = vsync-driven, no pacing applied).
  std::chrono::microseconds active_period{16667};  // ≈60 FPS
  /// Idle frame period when the loop reports idle (0 = no idle cap).
  std::chrono::microseconds idle_period{50000};  // ≈20 FPS
  /// Spin window kept for the caller's pause-loop before the deadline.
  std::chrono::microseconds spin_window{1500};
};

/// The split for one frame.
struct PaceDecision {
  /// Sleep this long before spinning (0 = spin immediately or don't pace).
  std::chrono::microseconds sleep_for{0};
  /// Spin the remaining window (caller loops _mm_pause until deadline).
  std::chrono::microseconds spin_for{0};
  /// False when pacing is disabled for this frame (period was 0).
  bool paced = false;
  /// True when the *idle* period was applied rather than the active one.
  bool idle = false;
};

/// Deadline for the next frame given the previous frame's start instant.
inline std::chrono::steady_clock::time_point next_deadline(
    std::chrono::steady_clock::time_point last_frame_start,
    std::chrono::microseconds period) {
  return last_frame_start + period;
}

/// Compute the sleep/spin split for one frame.
///
/// ``now`` is the current steady-clock instant (typically taken right after
/// the previous frame's present), ``last_frame_start`` the instant the
/// previous frame began, ``idle`` selects the idle period.
inline PaceDecision pace_frame(std::chrono::steady_clock::time_point now,
                               std::chrono::steady_clock::time_point last_frame_start,
                               const PaceConfig& config,
                               bool idle) {
  PaceDecision decision;
  const auto period = idle ? config.idle_period : config.active_period;
  if (period.count() <= 0) {
    decision.paced = false;
    return decision;
  }
  decision.paced = true;
  decision.idle = idle;
  const auto deadline = next_deadline(last_frame_start, period);
  auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
  if (remaining.count() <= 0) {
    // Already late: do not sleep, do not spin — run the frame immediately.
    return decision;
  }
  if (remaining <= config.spin_window) {
    decision.spin_for = remaining;
    return decision;
  }
  decision.sleep_for = remaining - config.spin_window;
  decision.spin_for = config.spin_window;
  return decision;
}

/// Should the loop apply the idle period this frame? The caller supplies its
/// own idle predicate (e.g. no entities for N consecutive frames, overlay
/// occluded); this helper only debounces: idle mode engages after
/// ``enter_after`` consecutive idle votes and disengages immediately on any
/// non-idle vote, so a single busy frame never pays the idle latency.
class IdleDebouncer {
 public:
  explicit IdleDebouncer(int enter_after = 30) : enter_after_(enter_after < 1 ? 1 : enter_after) {}

  bool idle() const { return idle_; }

  /// Feed one idle vote; returns the resulting idle state.
  bool vote(bool frame_was_idle) {
    if (!frame_was_idle) {
      idle_ = false;
      consecutive_ = 0;
      return false;
    }
    if (idle_) return true;
    if (++consecutive_ >= enter_after_) idle_ = true;
    return idle_;
  }

 private:
  int enter_after_ = 30;
  int consecutive_ = 0;
  bool idle_ = false;
};

}  // namespace real::cs2::pacing
