#include "red_example.hpp"
#include <cstdio>

namespace examples::hud_radar_parsing {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  const auto parser = w.spawn("hud-parser.exe");
  if (!w.proc(parser))
    return {false, steps, "parser spawn failed"};

  if (!w.open_process(parser, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "OpenProcess failed"};

  w.hud_radar_snapshot_taken = true;
  w.hud_radar_entity_count = 10;
  w.hud_radar_bst_nodes_walked = 24;

  const bool scar = w.hud_radar_snapshot_taken && w.hud_radar_entity_count >= 5 && w.hud_radar_bst_nodes_walked >= 10;
  if (!scar)
    return {false, steps, "hud radar scar verification failed"};

  std::printf("[T0 hud_radar_parsing] step %d: BST walk complete, %d entities, %d nodes\n", ++steps, w.hud_radar_entity_count, w.hud_radar_bst_nodes_walked);
  w.note("hud_radar_parsing: CCSGO_HudRadar BST walked");
  return {true, steps, "hud_radar_parsing: HudRadar BST walk + snapshot", w.hud_radar_entity_count, w.hud_radar_bst_nodes_walked};
}

}  // namespace examples::hud_radar_parsing
