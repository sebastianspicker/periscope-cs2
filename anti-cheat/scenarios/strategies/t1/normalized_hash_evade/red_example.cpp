#include "red_example.hpp"
#include <cstdio>

namespace examples::normalized_hash_evade {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  const auto evader = w.spawn("normalizer.exe");
  if (!w.proc(evader))
    return {false, steps, "evader spawn failed"};

  w.normalized_hash_evade_active = true;
  w.pe_hash_normalized = true;
  w.pe_section_entries_altered = 3;

  for (auto& m : game->modules)
    m.text_hash = "normalized";

  const bool scar = w.normalized_hash_evade_active && w.pe_hash_normalized && w.pe_section_entries_altered >= 2;
  if (!scar)
    return {false, steps, "normalized hash evade scar failed"};

  std::printf("[T1 normalized_hash_evade] step %d: PE hash normalized, %d sections altered\n", ++steps, w.pe_section_entries_altered);
  w.note("normalized_hash_evade: VAC normalized PE hash evasion");
  return {true, steps, "normalized_hash_evade: PE hash normalized", true, w.pe_section_entries_altered};
}

}  // namespace examples::normalized_hash_evade
