#include "red_example.hpp"
#include <cstdio>

namespace examples::vas_walk_evade_phase {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto evader = w.spawn("vas-evader.exe");
  if (!w.proc(evader))
    return {false, steps, "evader spawn failed"};

  w.vas_walk_evade_phase_active = true;
  w.vas_walk_evasion_count = 7;
  w.vas_page_hidden = true;
  w.vas_region_reshuffled = true;

  const bool scar = w.vas_walk_evade_phase_active && w.vas_page_hidden && w.vas_region_reshuffled;
  if (!scar)
    return {false, steps, "VAS walk evade phase scar failed"};

  std::printf("[T1 vas_walk_evade_phase] step %d: VAS walk evasion count=%d, page hidden, region reshuffled\n", ++steps, w.vas_walk_evasion_count);
  w.note("vas_walk_evade_phase: VAS walk phase detection + page hiding");
  return {true, steps, "vas_walk_evade_phase: page hidden + region reshuffled", w.vas_walk_evasion_count, true, true};
}

}  // namespace examples::vas_walk_evade_phase
