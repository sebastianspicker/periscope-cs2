#include "red_example.hpp"

namespace examples::delayed_ban {

// RED: stay under instant thresholds — weak scars across multiple match ticks.
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("short-life-radar.exe");
  // Weak handle only (no rage); private build id.
  w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  w.binary_build_id = "private-buyer-build";
  // Multi-tick: each sample adds weak confidence (not enough alone).
  w.lab_match_tick = 0;
  w.lab_confidence = 0;
  w.lab_delayed_ban_ready = false;
  w.advance_match_tick(0.8);  // tick 1 — under ban bar
  w.advance_match_tick(0.9);  // tick 2 — crosses multi-sample bar
  w.advance_match_tick(0.5);  // tick 3
  r.ticks = w.lab_match_tick;
  r.confidence = w.lab_confidence;
  r.achieved = r.ticks >= 2 && r.confidence > 0 && !w.ranked_access_denied;
  r.detail = "delayed_ban red ticks=" + std::to_string(r.ticks) +
             " conf=" + std::to_string(r.confidence) + " build=" + w.binary_build_id;
  w.note(r.detail);
  return r;
}

}  // namespace examples::delayed_ban
