// spot_ttl.hpp: Recently-spotted TTL for the hud-echo intel gate (E49).
//
// `m_bSpotted` is not a stable boolean: the game pulses it, and a raw read
// makes blips flicker. A flickering blip is worse for a low-vision user than
// a slightly stale one, so the live loop remembers the last true observation
// for a short window instead of drawing the raw flag.
//
// Two modes are deliberately supported:
//   - ttl_ms > 0: recently-spotted memory (default 1000 ms)
//   - ttl_ms == 0: strict, no memory, unknown/current-false hides
//
// `known == false` (the read failed) is never the same as "spotted"; strict
// intel hides on unknown so a failed read cannot leak an unspotted enemy.
//
// Pure and host-testable: no platform headers, no clocks.

#pragma once

#include <cstdint>
#include <unordered_map>

namespace real::cs2::spot {

class SpotTtl {
 public:
  explicit SpotTtl(std::uint64_t ttl_ms = 1000): ttl_ms_(ttl_ms) {}

  void set_ttl_ms(std::uint64_t ttl_ms) { ttl_ms_ = ttl_ms; }
  std::uint64_t ttl_ms() const { return ttl_ms_; }

  /// Feed one observation. A true+known observation refreshes the window.
  void observe(std::uint32_t handle, bool known, bool spotted,
               std::uint64_t now_ms) {
    if (handle == 0) return;
    if (known && spotted) {
      last_true_[handle] = now_ms;
    } else if (known) {
      // Explicit false: keep any existing window, but ensure the handle has an
      // entry so prune/size reflect it. Strict mode uses the caller's `known`
      // argument, not map presence.
      last_true_.try_emplace(handle, 0);
    }
  }

  /// True when the handle counts as visible now. Unknown handles are false.
  bool visible(std::uint32_t handle, bool known, bool spotted,
               std::uint64_t now_ms) const {
    if (!known) return false;
    if (spotted) return true;
    if (ttl_ms_ == 0) return false;
    const auto it = last_true_.find(handle);
    if (it == last_true_.end()) return false;
    return now_ms >= it->second && now_ms - it->second <= ttl_ms_;
  }

  /// Drop entries whose window has expired (bounded memory).
  void prune(std::uint64_t now_ms) {
    if (ttl_ms_ == 0) {
      last_true_.clear();
      return;
    }
    for (auto it = last_true_.begin(); it != last_true_.end();) {
      if (now_ms >= it->second && now_ms - it->second > ttl_ms_) {
        it = last_true_.erase(it);
      } else {
        ++it;
      }
    }
  }

  std::size_t size() const { return last_true_.size(); }
  void clear() { last_true_.clear(); }

 private:
  std::uint64_t ttl_ms_;
  std::unordered_map<std::uint32_t, std::uint64_t> last_true_;
};

}  // namespace real::cs2::spot
