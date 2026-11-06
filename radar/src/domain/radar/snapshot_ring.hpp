// snapshot_ring.hpp — R3: lock-free snapshot publication between workers.
//
// Adapted from cs2-dma (Apache-2.0, NOTICE recorded): a dual-worker pipeline
// (data worker + camera worker) exchanges state through a fixed ring of
// snapshot slots published by atomic sequence counters, plus a scene-serial
// watermark so a map change invalidates all stale state in one flip.
//
// Documented upstream failure lesson preserved here as an invariant: there is
// exactly ONE continuity owner (the data worker). A second cache in front of
// the ring would restart its grace period on every poll, resurrect rejected
// slots and extend stale frames indefinitely when the producer stops.
//
// Header-only, threads via <atomic> only, no locks on the hot path. The
// simulation feeds pre-built rings, so no thread is created here.

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace radar_domain {

// Seqlock-per-slot ring: a single writer publishes snapshots, any number of
// readers copy the newest complete slot. Readers retry on a torn copy; a
// writer that laps a slow reader simply causes retries, never corruption.
template <typename T, std::size_t kSlots>
class SnapshotRing {
  static_assert(kSlots >= 2, "a ring needs at least two slots to lap safely");

 public:
  SnapshotRing() = default;
  SnapshotRing(const SnapshotRing&) = delete;
  SnapshotRing& operator=(const SnapshotRing&) = delete;

  // Writer side: single thread at a time. Copies `payload` into the next
  // slot and publishes it.
  void publish(const T& payload) {
    Slot& slot = slots_[write_slot_];
    const std::uint64_t s0 = slot.seq.load(std::memory_order_relaxed);
    slot.seq.store(s0 + 1, std::memory_order_relaxed);  // odd: writing
    // Release fence: on weakly ordered hosts (ARM) plain payload stores may
    // otherwise become visible before the odd sequence store, letting a
    // reader that samples the old even value twice accept a torn copy.
    std::atomic_thread_fence(std::memory_order_release);
    // Feed the payload bytes through the slot. Trivially-copyable payloads
    // (the intended use) memcpy; anything else uses the copy-assign path.
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memcpy(static_cast<void*>(&slot.payload), static_cast<const void*>(&payload),
                  sizeof(T));
    } else {
      slot.payload = payload;
    }
    slot.seq.store(s0 + 2, std::memory_order_release);  // even: stable
    write_slot_ = (write_slot_ + 1) % kSlots;
    published_.fetch_add(1, std::memory_order_release);
  }

  // Reader side: copies the newest complete snapshot into `out`.
  // Returns false when nothing has been published yet.
  bool read_latest(T& out) const {
    const std::uint64_t completed = published_.load(std::memory_order_acquire);
    if (completed == 0) {
      return false;
    }
    const Slot& slot = slots_[(completed - 1) % kSlots];
    for (int retry = 0; retry < kMaxRetries; ++retry) {
      const std::uint64_t s1 = slot.seq.load(std::memory_order_acquire);
      if ((s1 & 1u) != 0u) {
        continue;  // writer mid-copy on this slot
      }
      if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(static_cast<void*>(&out), static_cast<const void*>(&slot.payload),
                    sizeof(T));
      } else {
        out = slot.payload;
      }
      const std::uint64_t s2 = slot.seq.load(std::memory_order_acquire);
      if (s1 == s2) {
        return true;
      }
    }
    return false;  // writer is lapping faster than we can copy
  }

  std::uint64_t published() const {
    return published_.load(std::memory_order_acquire);
  }

 private:
  static constexpr int kMaxRetries = 8;

  struct Slot {
    std::atomic<std::uint64_t> seq{0};
    T payload{};
  };

  Slot slots_[kSlots]{};
  std::atomic<std::uint64_t> published_{0};
  std::size_t write_slot_ = 0;
};

// Scene-serial watermark: on map change the owner bumps the serial once, and
// every snapshot stamped with an older serial is stale. Nothing heals across
// scenes.
class SceneWatermark {
 public:
  std::uint64_t current() const {
    return serial_.load(std::memory_order_acquire);
  }

  // Call once per scene transition (map change, disconnect, reconnect).
  std::uint64_t invalidate() {
    return serial_.fetch_add(1, std::memory_order_release) + 1;
  }

  [[nodiscard]] bool is_fresh(std::uint64_t snapshot_serial) const {
    return snapshot_serial == serial_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<std::uint64_t> serial_{1};
};

// Freshness gate: a sampled value older than `max_age_ms` is stale, and a
// sampled-at stamp from the future is stale too (monotonicity check).
[[nodiscard]] inline bool fresh_by_age(std::uint64_t sampled_at_ms,
                                       std::uint64_t now_ms,
                                       std::uint64_t max_age_ms) noexcept {
  return sampled_at_ms <= now_ms && now_ms - sampled_at_ms <= max_age_ms;
}

}  // namespace radar_domain
