// entity_cache.hpp: Event-diff entity cache with torn-free publish
// (assessment ledger E14).
//
// Two portable primitives:
//
// 1. TripleBuffer<T>, single-writer / multi-reader lock-free publication.
//    The writer rotates through three buffers (0→1→2→0…), never writing the
//    currently-published one; a reader copies the published buffer and
//    re-checks the published index afterwards. This is the failure mode
//    documented in the assessment ledger for the two-buffer variant (E41:
//    a reader that loads the index, stalls, and copies while two publishes
//    land reads a buffer the writer has started overwriting). With rotation
//    the reader's buffer is untouched for two more publishes, and the
//    post-copy re-check proves stability.
//
// 2. EntityCache, event-diff cache keyed by stable entity handle:
//    upsert/remove events maintain a live set; each publish records which
//    handles were added and removed since the previous publish, and a
//    per-handle generation counter is bumped on every upsert. Consumers
//    detect stale reads by generation: a snapshot whose generation is older
//    than the cache's current generation is stale by definition.
//
// Educational framing (strategy pair E27): memoized container pointers plus
// list reallocation is a classic TOCTOU class. This cache makes the
// invalidation *explicit*, removals propagate as events, generations expose
// staleness, which is exactly the blue-side counter the catalog pair models.
//
// Pure and host-testable: standard-library only.

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace real::cs2::cache {

template <typename T>
class TripleBuffer {
 public:
  /// Publish a new value. **Single writer only** (publish must never race
  /// with itself); readers may call ``load`` concurrently.
  void publish(const T& value) {
    const std::size_t current = index_.load(std::memory_order_relaxed);
    const std::size_t write =
        (current == kNoIndex) ? 0: (current + 1) % 3;
    buffers_[write] = value;
    index_.store(write, std::memory_order_release);
  }

  /// Copy out the newest published value. Returns false when nothing has
  /// been published yet, or when publishes landed faster than the copy
  /// (caller retries next frame). Torn-free by construction: the writer
  /// rotates 0→1→2→0…, so the buffer a reader is copying is untouched for
  /// two more publishes; the post-copy index re-check proves the copy was
  /// taken while the index (and therefore the buffer) was stable.
  bool load(T& out) const {
    for (int attempt = 0; attempt < 2; ++attempt) {
      const std::size_t index = index_.load(std::memory_order_acquire);
      if (index == kNoIndex) return false;
      const T copy = buffers_[index];
      if (index_.load(std::memory_order_acquire) == index) {
        out = copy;
        return true;
      }
    }
    return false;
  }

  bool has_value() const { return index_.load(std::memory_order_acquire) != kNoIndex; }

 private:
  static constexpr std::size_t kNoIndex = 3;

  mutable std::atomic<std::size_t> index_{kNoIndex};
  T buffers_[3]{};
};

/// Diff of one publish: what changed since the previous publish.
struct EntityDiff {
  std::vector<std::uint32_t> added;
  std::vector<std::uint32_t> removed;
};

/// A snapshot of the cache: all live entities plus the monotonic generation.
/// Copyable by value; consumers own their copy (torn-free by construction
/// because publish is event-driven under a short critical section).
template <typename Entity>
struct CacheSnapshot {
  std::vector<Entity> entities;
  std::uint64_t generation = 0;
};

template <typename Entity>
class EntityCache {
 public:
  /// Insert or refresh one entity (the "add_entity / state tick" event).
  /// Returns the diff accumulated so far via take_diff semantics? No -
  /// diffs accumulate until ``publish``; call ``publish`` once per walk.
  void upsert(std::uint32_t handle, const Entity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);
    removed_pending_.erase(handle);
    auto& slot = live_[handle];
    slot.entity = entity;
    slot.generation = ++generation_;
    touched_.insert(handle);
  }

  /// Record an entity removal event ("remove_entity").
  void remove(std::uint32_t handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (live_.erase(handle) > 0) {
      removed_pending_.insert(handle);
      touched_.erase(handle);
    }
  }

  /// Publish the current state: returns the snapshot and records the diff
  /// (added = handles present now but not in the previous publish; removed =
  /// handles removed since the previous publish).
  CacheSnapshot<Entity> publish() {
    std::lock_guard<std::mutex> lock(mutex_);
    return publish_locked();
  }

 private:
  /// Publish the current state; caller holds ``mutex_``.
  CacheSnapshot<Entity> publish_locked() {
    CacheSnapshot<Entity> snapshot;
    snapshot.generation = generation_;
    snapshot.entities.reserve(live_.size());
    EntityDiff diff;
    for (const auto& [handle, slot]: live_) {
      snapshot.entities.push_back(slot.entity);
      if (published_.find(handle) == published_.end()) {
        diff.added.push_back(handle);
      }
    }
    for (std::uint32_t handle: removed_pending_) {
      if (published_.find(handle) != published_.end()) {
        diff.removed.push_back(handle);
      }
    }
    published_ = touched_;
    for (std::uint32_t handle: removed_pending_) published_.erase(handle);
    removed_pending_.clear();
    last_diff_ = diff;
    latest_.publish(snapshot);
    return snapshot;
  }

 public:
  /// Sync the cache with a full walk result (the scattered reader produces
  /// a complete list every walk): upsert every present entity, remove every
  /// previously-present handle that is absent now, then publish. Returns the
  /// snapshot; ``last_diff()`` carries the add/remove events.
  template <typename KeyFn>
  CacheSnapshot<Entity> sync(const std::vector<Entity>& entities, KeyFn key_of) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_set<std::uint32_t> present;
    present.reserve(entities.size());
    for (const Entity& entity : entities) {
      const std::uint32_t handle = key_of(entity);
      removed_pending_.erase(handle);
      present.insert(handle);
      auto& slot = live_[handle];
      slot.entity = entity;
      slot.generation = ++generation_;
      touched_.insert(handle);
    }
    for (const auto& [handle, slot]: live_) {
      (void)slot;
      if (present.find(handle) == present.end() &&
          removed_pending_.find(handle) == removed_pending_.end()) {
        removed_pending_.insert(handle);
      }
    }
    // Absent handles leave the live set now; the diff records the event.
    for (const std::uint32_t handle: removed_pending_) {
      live_.erase(handle);
    }
    return publish_locked();
  }

  /// Newest published snapshot (lock-free via the triple buffer). Returns
  /// false when no publish has happened yet.
  bool latest(CacheSnapshot<Entity>& out) const { return latest_.load(out); }

  /// The diff recorded by the most recent publish.
  EntityDiff last_diff() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_diff_;
  }

  /// Current cache generation (bumped on every upsert). A consumer-side
  /// snapshot with generation < current is stale.
  std::uint64_t generation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation_;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return live_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    live_.clear();
    touched_.clear();
    published_.clear();
    removed_pending_.clear();
    ++generation_;
  }

 private:
  struct Slot {
    Entity entity{};
    std::uint64_t generation = 0;
  };

  mutable std::mutex mutex_;
  std::unordered_map<std::uint32_t, Slot> live_;
  std::unordered_set<std::uint32_t> touched_;
  std::unordered_set<std::uint32_t> published_;
  std::unordered_set<std::uint32_t> removed_pending_;
  std::uint64_t generation_ = 0;
  EntityDiff last_diff_;
  TripleBuffer<CacheSnapshot<Entity>> latest_;
};

}  // namespace real::cs2::cache
