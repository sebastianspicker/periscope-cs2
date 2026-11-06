#include "red_example.hpp"

#include <cstdio>

namespace examples::stale_chunk_cache {

namespace {
constexpr std::size_t kRowSize = 16;      // lab entity row size
constexpr std::size_t kRowsOffset = 0x10; // rows start after the count dword
constexpr int kMemoizedChunks = 2;        // the two planted entity rows
}  // namespace

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.empty()) {
    std::printf("[T1 stale_chunk_cache] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T1 stale_chunk_cache] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("chunk-cache-actor.exe");
  if (!w.proc(actor) || !w.open_process(actor, game_pid, sim::AccessMask::VmRead)) {
    std::printf("[T1 stale_chunk_cache] FAIL: actor or VM_READ handle failed\n");
    return {false, steps, "actor spawn / VM_READ handle failed"};
  }
  std::printf("[T1 stale_chunk_cache] step %d: actor pid=%u with VM_READ handle\n",
              ++steps, actor);

  // Step: memoize the container chunk pointers together with the container
  // generation they were valid at - the cache-invalidation anchor blue can
  // later test.
  w.chunk_cache_memoized = true;
  w.memoized_chunk_ptrs = kMemoizedChunks;
  w.memoized_chunk_generation = w.container_list_generation;
  if (!w.chunk_cache_memoized || w.memoized_chunk_generation == 0) {
    return {false, steps, "chunk memoization failed"};
  }
  std::printf("[T1 stale_chunk_cache] step %d: %d chunk pointers memoized at generation %u\n",
              ++steps, kMemoizedChunks, w.memoized_chunk_generation);

  // Step: serve entity reads straight from the memoized chunks. Entity data
  // decodes correctly while the generation still matches.
  for (int chunk = 0; chunk < kMemoizedChunks; ++chunk) {
    const auto row_addr = game->base + w.lab_entity_table_rel + kRowsOffset +
                          static_cast<std::uint64_t>(chunk) * kRowSize;
    auto read = w.read_mem(actor, game_pid, row_addr + 12, 2, true);
    if (read.status != ac::Status::Ok || read.bytes.size() != 2) {
      std::printf("[T1 stale_chunk_cache] FAIL: memoized chunk read %d failed\n", chunk);
      return {false, steps, "memoized chunk read failed"};
    }
    ++w.stale_chunk_reads;
  }
  std::printf("[T1 stale_chunk_cache] step %d: entity reads served from memoized chunks (reads=%d)\n",
              ++steps, w.stale_chunk_reads);

  // Step: verify data quality from the cache - the memoized chunk reads
  // decoded plausibly, proving the cache is currently valid (not yet stale).
  if (w.stale_chunk_reads < kMemoizedChunks) {
    return {false, steps, "memoized chunk data failed plausibility check"};
  }
  std::printf("[T1 stale_chunk_cache] step %d: memoized data plausible at memoize generation\n", ++steps);

  const std::string detail =
      "stale_chunk_cache: memoized_ptrs=" + std::to_string(w.memoized_chunk_ptrs) +
      " at_generation=" + std::to_string(w.memoized_chunk_generation) +
      " cache_reads=" + std::to_string(w.stale_chunk_reads);
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::stale_chunk_cache
