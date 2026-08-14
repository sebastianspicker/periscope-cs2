#include "red_example.hpp"
#include <cstdio>

namespace examples::forensic_cleanup_exit {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto cleaner = w.spawn("cleanup.exe");
  if (!w.proc(cleaner))
    return {false, steps, "cleanup spawn failed"};

  w.forensic_cleanup_active = true;
  w.forensic_cleanup_steps_completed = 3;
  w.forensic_prefetch_cleared = true;
  w.forensic_recent_cleared = true;

  const bool scar = w.forensic_cleanup_active && w.forensic_cleanup_steps_completed >= 3 && w.forensic_prefetch_cleared && w.forensic_recent_cleared;
  if (!scar)
    return {false, steps, "forensic cleanup scar failed"};

  std::printf("[T0 forensic_cleanup_exit] step %d: %d cleanup steps completed\n", ++steps, w.forensic_cleanup_steps_completed);
  w.note("forensic_cleanup_exit: prefetch/registry/recent cleanup");
  return {true, steps, "forensic_cleanup_exit: 3 cleanup steps", w.forensic_cleanup_steps_completed, true, true};
}

}  // namespace examples::forensic_cleanup_exit
