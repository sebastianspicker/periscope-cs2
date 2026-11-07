// scatter_batch.hpp: R10: DMA scatter-read discipline (portable policies).
//
// Adapted from the research ledger (R10): oasis batch sizing ("reads beyond
// what the FPGA can keep in flight stall for several milliseconds") and
// cs2-dma recovery policies (pointer-cache revalidation, eviction, recovery
// windows). This is the pure half, batching, revalidation, and latency
// modeling, with no hardware access. The live DMA adapter consumes these
// policies under the existing opt-in lab flags.
//
// The pointer-cache rule (oasis): cache each entity-slot's resolved pointer
// between frames, re-read the pointer together with the data in batched
// scatter reads, and only use the data when the pointer did not change.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace radar_domain {

// One entity-slot pointer-cache entry carried between frames.
struct PointerCacheEntry {
  std::uint32_t entity_slot = 0;
  std::uint64_t pointer = 0;        // resolved pawn/entity pointer
  std::uint32_t generation = 0;     // slot generation observed last frame
  bool valid = false;               // entry usable at all
};

// Result of re-reading one entry's pointer (and generation) this frame.
struct PointerRevalidation {
  std::uint32_t entity_slot = 0;
  std::uint64_t pointer = 0;
  std::uint32_t generation = 0;
  bool read_ok = false;
};

// Outcome classes for a revalidated entry (cs2-dma policy vocabulary).
enum class RevalidationOutcome: std::uint8_t {
  Unchanged = 0,   // pointer and generation stable: payload may be used
  Moved = 1,       // pointer changed: evict and re-resolve next frame
  GenerationGap = 2,  // slot generation moved: treat as respawn/reset
  ReadFailed = 3,  // the pointer re-read failed: drop for this frame
};

RevalidationOutcome classify_revalidation(
    const PointerCacheEntry& cached, const PointerRevalidation& fresh);

// Partitions entries into scatter batches of at most `max_in_flight`.
// Batches beyond the hardware's in-flight capacity stall the pipeline
// (oasis), so the planner never emits an oversized batch. Entries are
// grouped in slot order; an invalid cache entry is still included (its
// pointer re-read is what revalidates it), but failed reads never enter a
// payload batch.
std::vector<std::vector<PointerCacheEntry>> plan_scatter_batches(
    const std::vector<PointerCacheEntry>& entries, std::size_t max_in_flight);

// Latency model for a simulated device (test/oracle use): a batch costs
// base_latency_ms plus a stall penalty when it exceeds in-flight capacity.
struct DeviceLatencyModel {
  double base_latency_ms = 1.0;
  double per_read_ms = 0.01;
  double stall_penalty_ms = 4.0;  // applied once per oversized batch
  std::size_t in_flight_capacity = 32;

  [[nodiscard]] double batch_latency_ms(std::size_t reads) const;
};

// cs2-dma recovery-window vocabulary: after a bulk eviction (scene change,
// device reset) reads degrade for a bounded window; consumers hold the last
// good frame instead of flapping.
[[nodiscard]] inline bool within_bulk_recovery_window(
    std::uint64_t eviction_us, std::uint64_t now_us,
    std::uint64_t window_us) noexcept {
  return eviction_us != 0 && now_us >= eviction_us && now_us - eviction_us < window_us;
}

}  // namespace radar_domain
