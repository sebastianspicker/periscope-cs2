// radar_scatter_test.cpp: R10: scatter-batch discipline against a simulated
// device with latency injection (no hardware, portable).

#include "radar/scatter_batch.hpp"

#include <cstdio>
#include <numeric>

namespace {

using radar_domain::DeviceLatencyModel;
using radar_domain::PointerCacheEntry;
using radar_domain::PointerRevalidation;
using radar_domain::RevalidationOutcome;
using radar_domain::classify_revalidation;
using radar_domain::plan_scatter_batches;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

PointerCacheEntry entry(std::uint32_t slot, std::uint64_t pointer,
                        std::uint32_t generation, bool valid) {
  return PointerCacheEntry{slot, pointer, generation, valid};
}

void test_batching_respects_capacity() {
  std::vector<PointerCacheEntry> entries;
  for (std::uint32_t slot = 0; slot < 100; ++slot) {
    entries.push_back(entry(slot, 0x1000 + slot, 1, true));
  }

  const auto batches = plan_scatter_batches(entries, 32);
  check(batches.size() == 4, "100 entries / 32 capacity = 4 batches");
  for (const auto& batch : batches) {
    check(batch.size() <= 32, "no oversized batch emitted");
  }
  // Slot order preserved across batch boundaries.
  check(batches.front().front().entity_slot == 0, "first slot first");
  check(batches.back().back().entity_slot == 99, "last slot last");

  // A naive single batch of 100 reads stalls; each planner batch does not.
  const DeviceLatencyModel model;
  const double naive = model.batch_latency_ms(100);
  double planned = 0.0;
  for (const auto& batch : batches) {
    planned += model.batch_latency_ms(batch.size());
  }
  check(naive > planned, "planner beats naive oversized batch");
  check(model.batch_latency_ms(32) < model.batch_latency_ms(100),
        "capacity-sized batch cheaper than stalling one");
}

void test_batching_edges() {
  check(plan_scatter_batches({}, 32).empty(), "empty input -> no batches");
  std::vector<PointerCacheEntry> single{entry(0, 1, 1, true)};
  const auto one = plan_scatter_batches(single, 32);
  check(one.size() == 1 && one[0].size() == 1, "single entry single batch");
  check(plan_scatter_batches(single, 0).empty(),
        "zero capacity refuses to batch (never oversized)");
}

void test_revalidation_classification() {
  const PointerCacheEntry cached = entry(7, 0xABC0, 5, true);

  check(classify_revalidation(
            cached, PointerRevalidation{7, 0xABC0, 5, true}) ==
            RevalidationOutcome::Unchanged,
        "stable pointer and generation");

  check(classify_revalidation(
            cached, PointerRevalidation{7, 0xDEF0, 5, true}) ==
            RevalidationOutcome::Moved,
        "pointer moved -> evict");

  check(classify_revalidation(
            cached, PointerRevalidation{7, 0xABC0, 6, true}) ==
            RevalidationOutcome::GenerationGap,
        "generation gap -> respawn semantics");

  check(classify_revalidation(
            cached, PointerRevalidation{7, 0xABC0, 5, false}) ==
            RevalidationOutcome::ReadFailed,
        "read failure -> drop frame");

  // An invalid cache entry whose re-read succeeds is "moved" (fresh resolve),
  // never "unchanged".
  const PointerCacheEntry invalid = entry(8, 0, 0, false);
  check(classify_revalidation(
            invalid, PointerRevalidation{8, 0x1234, 9, true}) ==
            RevalidationOutcome::Moved,
        "invalid cache entry re-resolves");
}

void test_recovery_window() {
  check(radar_domain::within_bulk_recovery_window(1000, 1500, 600),
        "inside recovery window");
  check(!radar_domain::within_bulk_recovery_window(1000, 1600, 600),
        "window expired");
  check(!radar_domain::within_bulk_recovery_window(0, 1000, 600),
        "no eviction recorded -> no window");
  check(!radar_domain::within_bulk_recovery_window(2000, 1000, 600),
        "future eviction -> no window");
}

}  // namespace

int main() {
  test_batching_respects_capacity();
  test_batching_edges();
  test_revalidation_classification();
  test_recovery_window();

  if (failures != 0) {
    std::printf("%d scatter check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_scatter_test: all checks passed\n");
  return 0;
}
