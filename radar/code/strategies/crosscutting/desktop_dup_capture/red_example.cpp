#include "red_example.hpp"
#include <sstream>
namespace examples::desktop_dup_capture {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("dup-aware-esp.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  sim::OverlayWindow ov;
  ov.owner_pid = r.actor_pid;
  ov.title = "ESP-present";
  ov.topmost = true;
  ov.transparent = true;
  ov.stream_proof = true;
  ov.band4_zorder = true;
  w.add_overlay(ov);
  // Present path has overlay; capture path initially blind (streamproof class).
  w.present_path_has_overlay = true;
  w.capture_sees_overlays = false;
  w.desktop_duplication = true;  // hardware/API residual for capture sensor
  // Red does not activate AC capture sensor — blue will.
  w.capture_sensor_active = false;
  w.capture_vs_present_mismatch = false;
  r.achieved = w.present_path_has_overlay && !w.capture_sees_overlays &&
               w.desktop_duplication;
  std::ostringstream oss;
  oss << "desktop_dup_capture red present_overlay=1 capture_sees=0 "
      << "desktop_duplication=1 (sensor armed by blue)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::desktop_dup_capture
