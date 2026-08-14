// RED: multi-step band-4 stream-proof overlay visible on present, invisible to capture.

#include "red_example.hpp"

#include <sstream>

namespace examples::streamproof_overlay {

RedResult apply(sim::World& w) {
  RedResult r;

  // Phase 1: external ESP actor + game VM_READ handle.
  r.actor_pid = w.spawn("streamproof-esp.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  ++r.steps;

  // Phase 2: plant stream-proof band-4 overlay on present path.
  sim::OverlayWindow ov;
  ov.owner_pid = r.actor_pid;
  ov.title = "SP-ESP";
  ov.topmost = true;
  ov.transparent = true;
  ov.stream_proof = true;
  ov.band4_zorder = true;
  ov.z_order = sim::ZOrderBand::Band4;
  w.add_overlay(ov);
  ++r.steps;

  // Phase 3: capture/OBS path misses band-4 stream-proof window.
  w.capture_sees_overlays = false;
  ++r.steps;

  // Phase 4: desktop-duplication capture sensor active (present vs capture story).
  w.desktop_duplication = true;
  w.desktop_duplication_active = true;
  ++r.steps;

  r.achieved = !w.overlays.empty() && w.overlays.back().stream_proof &&
               w.overlays.back().band4_zorder && !w.capture_sees_overlays &&
               r.steps >= 2;
  std::ostringstream oss;
  oss << "streamproof_overlay red steps=" << r.steps
      << " stream_proof=1 band4=1 capture_sees=0 desktop_dup=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::streamproof_overlay
