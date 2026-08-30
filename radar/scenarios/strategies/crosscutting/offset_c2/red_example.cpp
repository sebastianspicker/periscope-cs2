// RED: remote offset/schema C2 product — versioned layout fetch.
// Distinct residual: net looks_like_offset_c2 + schema_remote_update pipeline.

#include "red_example.hpp"

#include <sstream>

namespace examples::offset_c2 {

RedResult apply(sim::World& w) {
  RedResult r;
  const auto actor = w.spawn("offset-c2-client.exe");
  r.actor_pid = actor;

  // Channel: open VmRead after applying remote schema (product order).
  w.add_net(sim::NetFlow{actor, "offsets.private-cdn.example:443",
                         /*looks_like_offset_c2=*/true,
                         /*looks_like_radar_saas=*/false});
  w.schema_remote_update = true;
  ++w.schema_fetch_count;
  w.schema_cache_active = true;
  w.schema_cache_version = "remote:" + w.binary_build_id + ":v2";
  ++w.schema_cache_hits;

  if (!w.open_process(actor, w.game_pid(), sim::AccessMask::VmRead, false)) {
    r.detail = "offset_c2 red open_process failed";
    w.note(r.detail);
    return r;
  }
  // Apply remote layout token: bump generation / plant entities if needed.
  const auto game = w.game_pid();
  if (auto* g = w.proc(game)) {
    if (!w.lab_pattern_marker_present || g->memory.size() < 0x208) {
      w.plant_lab_entities(game);
    }
    // Remote schema "moves" table (educational apply of C2 payload).
    (void)w.mutate_lab_pattern_layout(game, /*new_marker_off=*/0x240,
                                      /*new_table_rel=*/0x40);
    (void)w.read_mem(actor, game, g->base, 4, true);
  }

  r.achieved = w.schema_remote_update && w.schema_fetch_count >= 1 &&
               !w.schema_cache_version.empty();
  std::ostringstream oss;
  oss << "offset_c2 red remote_schema=" << w.schema_cache_version
      << " fetches=" << w.schema_fetch_count
      << " gen=" << w.lab_pattern_generation;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::offset_c2
