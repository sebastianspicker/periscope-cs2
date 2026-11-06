// dead_reckoning.hpp — Capped position extrapolation for throttled entities
// (E54).
//
// The live read throttle samples far/dormant entities at ~8 Hz. Rendering
// every sample raw makes those blips teleport once per 125 ms. This header
// extrapolates a *bounded* distance from the last sampled origin and velocity
// so the blip keeps moving smoothly, then snaps back on the next real sample.
//
// Rules that keep it honest:
//   - extrapolation never exceeds `max_ms` (default 200 ms);
//   - a velocity magnitude cap rejects teleport spikes;
//   - a new sample always wins over the extrapolated position;
//   - stationary entities extrapolate nothing.
//
// Pure and host-testable: no platform headers, no clocks.

#pragma once

#include "ac/types.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace real::cs2::reckon {

struct ReckonConfig {
  std::uint64_t max_ms = 200;
  float max_speed = 1200.0f;  ///< world units/s; above this, ignore velocity
};

class DeadReckoner {
 public:
  explicit DeadReckoner(ReckonConfig config = {}) : config_(config) {}

  /// Record a fresh sample (position and velocity in world units).
  void sample(std::uint32_t handle, const ac::Vec3& position,
              const ac::Vec3& velocity, std::uint64_t now_ms) {
    if (handle == 0) return;
    State& s = states_[handle];
    s.position = position;
    s.velocity = velocity;
    s.sampled_ms = now_ms;
    s.valid = true;
  }

  /// Extrapolated position, or false when no usable sample exists.
  bool position(std::uint32_t handle, std::uint64_t now_ms,
                ac::Vec3& out) const {
    const auto it = states_.find(handle);
    if (it == states_.end() || !it->second.valid) return false;
    const State& s = it->second;
    out = s.position;
    if (now_ms <= s.sampled_ms) return true;

    const std::uint64_t dt = now_ms - s.sampled_ms;
    if (dt == 0 || dt > config_.max_ms) return true;

    const float speed = std::sqrt(s.velocity.x * s.velocity.x +
                                  s.velocity.y * s.velocity.y +
                                  s.velocity.z * s.velocity.z);
    if (!std::isfinite(speed) || speed <= 0.05f || speed > config_.max_speed) {
      return true;
    }
    const float seconds = static_cast<float>(dt) / 1000.0f;
    out.x += s.velocity.x * seconds;
    out.y += s.velocity.y * seconds;
    out.z += s.velocity.z * seconds;
    return true;
  }

  void erase(std::uint32_t handle) { states_.erase(handle); }
  void clear() { states_.clear(); }
  std::size_t size() const { return states_.size(); }

 private:
  struct State {
    ac::Vec3 position{};
    ac::Vec3 velocity{};
    std::uint64_t sampled_ms = 0;
    bool valid = false;
  };

  ReckonConfig config_;
  std::unordered_map<std::uint32_t, State> states_;
};

}  // namespace real::cs2::reckon
