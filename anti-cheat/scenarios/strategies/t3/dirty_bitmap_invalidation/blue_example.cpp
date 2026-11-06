#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::dirty_bitmap_invalidation {

BlueResult detect(sim::World& w) {
  BlueResult result;
  // Signal shim: the shared add_signal takes const char*; wrap for std::string reasons.
  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present, reason.c_str());
  };
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  const auto* observer = w.proc(w.storm_observer_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    return result;
  }

  // Reason 1 (the leak): blue's own broadcast invalidation storm happened -
  // global flushes are observable system-wide events.
  sig(w.broadcast_invalidation_storm,
                      "broadcast invalidation storm leaked scan activity (events=" +
                          std::to_string(w.global_invalidation_events) + ")");
  // Reason 2 (strategy scar): a foreign observer watched the storm cadence.
  const bool foreign_observer =
      observer != nullptr && !observer->is_game && !observer->is_ac;
  sig(
                      w.storm_observation_active && foreign_observer,
                      "foreign observer correlated storm cadence (observations=" +
                          std::to_string(w.storm_observations) + ")");
  // Reason 3: the observer's reads are what triggered the flush storms.
  sig(w.remote_read_ops > 0,
                      "foreign read bursts preceded the storm (ops=" +
                          std::to_string(w.remote_read_ops) + ")");
  // Reason 4: cadence inference means red knows when/where blue scans.
  sig(
                      w.storm_observations >= w.global_invalidation_events,
                      "scan cadence inferred from storm cadence");

  const bool specific_scar = w.broadcast_invalidation_storm &&
                             w.storm_observation_active;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: broadcast-storm observation leaks blue scan intel");
  }

  // Mitigation: adopt dirty-bitmap selective invalidation. Only consumers
  // flagged in the dirty bitmap flush, so cadence no longer broadcasts.
  if (result.detected || specific_scar) {
    w.dirty_bitmap_invalidation_active = true;
    w.dirty_flagged_consumers = 2;
    w.dirty_selective_flushes = 3;
    result.reasons.emplace_back(
        "blue adopts dirty-bitmap selective invalidation (flagged consumers=" +
        std::to_string(w.dirty_flagged_consumers) + ", selective flushes=" +
        std::to_string(w.dirty_selective_flushes) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.dirty_bitmap_invalidation_active &&
                     result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "dirty_bitmap_invalidation blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T3 dirty_bitmap_invalidation] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T3 dirty_bitmap_invalidation]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::dirty_bitmap_invalidation
