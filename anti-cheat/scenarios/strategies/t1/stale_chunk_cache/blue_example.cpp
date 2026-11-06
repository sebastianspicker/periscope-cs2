#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::stale_chunk_cache {

BlueResult detect(sim::World& w) {
  BlueResult result;
  // Signal shim: the shared add_signal takes const char*; wrap for std::string reasons.
  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present, reason.c_str());
  };
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    return result;
  }

  // Blue's move: churn pressure. Deliberately reallocate the container so
  // every memoized chunk pointer in circulation ages out.
  w.container_reallocations = 2;
  w.container_list_generation += static_cast<std::uint32_t>(w.container_reallocations);
  w.chunk_generation_tagged = true;  // reallocations now bump generation counters
  sig(w.container_reallocations > 0,
                      "deliberate container churn: reallocations=" +
                          std::to_string(w.container_reallocations) +
                          " generation now=" +
                          std::to_string(w.container_list_generation));

  // Reason (strategy scar): a memoized chunk cache is in circulation, tagged
  // with the generation it was built at.
  sig(w.chunk_cache_memoized,
                      "memoized chunk cache present (ptrs=" +
                          std::to_string(w.memoized_chunk_ptrs) + " at generation " +
                          std::to_string(w.memoized_chunk_generation) + ")");

  // Reason: stale reads via generation counters. Every memoized pointer whose
  // generation predates the current one serves stale bytes after the churn.
  if (w.chunk_cache_memoized &&
      w.memoized_chunk_generation != w.container_list_generation) {
    w.stale_generation_reads = w.memoized_chunk_ptrs;
    sig(true,
        "stale reads observed: generation mismatch " +
            std::to_string(w.memoized_chunk_generation) + " vs " +
            std::to_string(w.container_list_generation) +
            " (" + std::to_string(w.stale_generation_reads) +
            " stale pointer reads)");
  }
  // Reason: the cache read telemetry from red's apply pass.
  sig(w.stale_chunk_reads > 0,
                      "chunk-cache read telemetry reads=" +
                          std::to_string(w.stale_chunk_reads));
  // Reason: generic remote-read telemetry from the delivery handle.
  sig(w.remote_read_ops > 0,
                      "remote read telemetry ops=" + std::to_string(w.remote_read_ops));

  const bool specific_scar = w.chunk_cache_memoized && w.stale_generation_reads > 0;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: stale chunk reads after blue-induced reallocation");
  }

  // Mitigation: generation counters are propagated to every chunk access, so
  // any consumer reading against an outdated generation is visible and must
  // re-validate before the next read is trusted.
  if (result.detected || specific_scar) {
    result.reasons.emplace_back(
        "blue propagates generation counters: stale consumers must re-validate");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.chunk_generation_tagged &&
                     result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "stale_chunk_cache blue signals=" +
                  std::to_string(result.signals) + " stale_reads=" +
                  std::to_string(w.stale_generation_reads) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T1 stale_chunk_cache] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T1 stale_chunk_cache]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::stale_chunk_cache
