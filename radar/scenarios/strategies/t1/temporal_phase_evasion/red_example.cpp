#include "red_example.hpp"
#include <cstdio>

namespace examples::temporal_phase_evasion {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto engine = w.spawn("temporal-engine.exe");
  if (!w.proc(engine))
    return {false, steps, "engine spawn failed"};

  w.temporal_phase_active = true;
  w.temporal_phase_index = 2;
  w.temporal_phase_transitions = 12;
  w.temporal_phase_ticks_in_phase = 8;

  const bool scar = w.temporal_phase_active && w.temporal_phase_transitions >= 4;
  if (!scar)
    return {false, steps, "temporal phase scar failed"};

  std::printf("[T1 temporal_phase_evasion] step %d: %d phase transitions, %d ticks in phase %d\n", ++steps, w.temporal_phase_transitions, w.temporal_phase_ticks_in_phase, w.temporal_phase_index);
  w.note("temporal_phase_evasion: 4-phase temporal jitter engine");
  return {true, steps, "temporal_phase_evasion: 4-phase jitter engine", w.temporal_phase_transitions, w.temporal_phase_ticks_in_phase};
}

}  // namespace examples::temporal_phase_evasion
