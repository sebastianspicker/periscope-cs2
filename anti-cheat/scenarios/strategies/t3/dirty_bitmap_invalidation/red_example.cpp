#include "red_example.hpp"

#include <cstdio>

namespace examples::dirty_bitmap_invalidation {

namespace {
// Each foreign read burst forces the validator to flush globally; the lab
// collapses "blue scan + global flush" into these storm events.
constexpr int kEventsPerBurst = 3;
constexpr int kBursts = 3;
constexpr int kStormThreshold = 6;
}  // namespace

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T3 dirty_bitmap_invalidation] FAIL: game process unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T3 dirty_bitmap_invalidation] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto observer = w.spawn("storm-observer.exe");
  if (!w.proc(observer)) {
    std::printf("[T3 dirty_bitmap_invalidation] FAIL: observer could not start\n");
    return {false, steps, "observer spawn verification failed"};
  }
  w.storm_observer_pid = observer;
  std::printf("[T3 dirty_bitmap_invalidation] step %d: storm observer pid=%u created\n",
              ++steps, observer);

  // Step: read bursts. In this lesson the validator answers every foreign
  // burst with a global (broadcast) invalidation, so each burst the lab
  // counts as global invalidation events.
  for (int burst = 0; burst < kBursts; ++burst) {
    auto read = w.read_mem(observer, game_pid, game->base + w.lab_entity_table_rel, 4, false);
    if (read.status != ac::Status::Ok) {
      std::printf("[T3 dirty_bitmap_invalidation] FAIL: read burst %d failed\n", burst);
      return {false, steps, "read burst failed"};
    }
    w.global_invalidation_events += kEventsPerBurst;
    std::printf("[T3 dirty_bitmap_invalidation] step %d: read burst %d forced %d global invalidation events\n",
                ++steps, burst + 1, kEventsPerBurst);
  }

  // Step: the storm. Once events pass the threshold, the global broadcast is
  // observable from any local sensor.
  if (w.global_invalidation_events < kStormThreshold) {
    return {false, steps, "invalidation storm threshold not reached"};
  }
  w.broadcast_invalidation_storm = true;
  std::printf("[T3 dirty_bitmap_invalidation] step %d: broadcast invalidation storm observed (%d events)\n",
              ++steps, w.global_invalidation_events);

  // Step: red watches the storm cadence and infers blue's scan schedule -
  // the information advantage this pair is about.
  w.storm_observation_active = true;
  w.storm_observations = w.global_invalidation_events;
  if (w.storm_observations <= 0) {
    return {false, steps, "storm observation failed"};
  }
  std::printf("[T3 dirty_bitmap_invalidation] step %d: scan cadence inferred from storm cadence\n", ++steps);

  const std::string detail =
      "dirty_bitmap_invalidation: storm_events=" +
      std::to_string(w.global_invalidation_events) +
      " observations=" + std::to_string(w.storm_observations) +
      " observer_pid=" + std::to_string(w.storm_observer_pid);
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::dirty_bitmap_invalidation
