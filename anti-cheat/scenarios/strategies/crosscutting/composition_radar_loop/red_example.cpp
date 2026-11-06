#include "red_example.hpp"
#include "strategies/crosscutting/overlay_esp/red_example.hpp"
#include "strategies/crosscutting/cs_round_radar/red_example.hpp"
#include "fps/scenario.hpp"
#include "strategies/t0/pattern_offset_scan/red_example.hpp"

namespace examples::composition_radar_loop {

// Multi-phase: CS scenario bridge product → pattern refresh lesson → overlay.
RedResult apply(sim::World& w) {
  RedResult r;

  // Phase 0: CS-inspired round entities into World (not static ACPT-only table).
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  auto cs = examples::cs_round_radar::apply_from_scenario(sc, w);
  r.actor_pid = cs.actor_pid;
  r.entities = cs.entity_count;

  // Phase 1–2: pattern scan + layout mutate + refresh (offset rot lesson).
  // Re-plant marker on current game memory for AOB path without wiping CS ents
  // if memory large enough — pattern scan uses plant_lab_entities if needed.
  examples::pattern_offset_scan::ScannerSession session;
  // Attach pattern scanner process on same World.
  auto scan = examples::pattern_offset_scan::apply(w, session);
  r.scanned = scan.achieved || cs.achieved;
  (void)w.mutate_lab_pattern_layout(w.game_pid(), 0x2C0, 0x80);
  auto ref = examples::pattern_offset_scan::refresh(w, session);
  r.refreshed = ref.achieved;

  // Phase 3: overlay product surface.
  auto ov = examples::overlay_esp::apply(w);
  r.achieved = cs.achieved && (r.scanned || r.refreshed) && ov.achieved &&
               r.entities >= 2;
  r.detail = "composition red cs_ents=" + std::to_string(cs.entity_count) +
             " site=" + cs.site +
             " scan=" + std::to_string(scan.achieved ? 1 : 0) +
             " refresh=" + std::to_string(r.refreshed ? 1 : 0) +
             " overlay=" + std::to_string(ov.achieved ? 1 : 0);
  w.note(r.detail);
  return r;
}

}  // namespace examples::composition_radar_loop
