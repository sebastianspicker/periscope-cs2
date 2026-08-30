#include "red_example.hpp"
#include <cstdio>

namespace examples::runtime_health_ladder {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto agent = w.spawn("health-agent.exe");
  if (!w.proc(agent))
    return {false, steps, "agent spawn failed"};

  w.health_ladder_level = 3;
  w.health_ladder_transition_count = 5;
  w.health_ladder_ticks_at_level = 10;
  w.health_ladder_self_heal_armed = true;

  const bool scar = w.health_ladder_level >= 3 && w.health_ladder_transition_count >= 3 && w.health_ladder_self_heal_armed;
  if (!scar)
    return {false, steps, "health ladder scar failed"};

  std::printf("[T0 runtime_health_ladder] step %d: health ladder at level %d after %d transitions\n", ++steps, w.health_ladder_level, w.health_ladder_transition_count);
  w.note("runtime_health_ladder: 3-level healing ladder active");
  return {true, steps, "runtime_health_ladder: level 3 with self-heal armed", w.health_ladder_level, w.health_ladder_transition_count};
}

}  // namespace examples::runtime_health_ladder
