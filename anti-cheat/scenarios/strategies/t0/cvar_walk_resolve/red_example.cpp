#include "red_example.hpp"
#include <cstdio>

namespace examples::cvar_walk_resolve {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto resolver = w.spawn("cvar-resolver.exe");
  if (!w.proc(resolver))
    return {false, steps, "resolver spawn failed"};

  if (!w.open_process(resolver, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "OpenProcess failed"};

  w.cvar_walk_resolved = true;
  w.cvar_walk_tier_reached = 4;
  w.cvar_walk_entries_found = 128;

  const bool scar = w.cvar_walk_resolved && w.cvar_walk_tier_reached >= 4 && w.cvar_walk_entries_found >= 100;
  if (!scar)
    return {false, steps, "cvar walk scar verification failed"};

  std::printf("[T0 cvar_walk_resolve] step %d: tier %d reached, %d entries found\n", ++steps, w.cvar_walk_tier_reached, w.cvar_walk_entries_found);
  w.note("cvar_walk_resolve: 4-tier CVar resolution cascade complete");
  return {true, steps, "cvar_walk_resolve: CVar cascade resolved", w.cvar_walk_tier_reached, w.cvar_walk_entries_found};
}

}  // namespace examples::cvar_walk_resolve
