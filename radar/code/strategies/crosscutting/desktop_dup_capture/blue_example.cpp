#include "blue_example.hpp"
#include <sstream>
namespace examples::desktop_dup_capture {
BlueResult detect(sim::World& w) {
  BlueResult r;
  // Blue arms DXGI desktop-duplication as *capture sensor* (AC path).
  w.capture_sensor_active = w.desktop_duplication;
  const bool present = w.present_path_has_overlay || !w.overlays.empty();
  const bool capture_empty = !w.capture_sees_overlays;
  if (w.capture_sensor_active && present && capture_empty) {
    w.capture_vs_present_mismatch = true;
  }
  r.detected = w.capture_sensor_active && w.capture_vs_present_mismatch;
  r.mitigated = r.detected && present && capture_empty;
  if (r.mitigated) {
    // Require capture parity with present path.
    w.capture_sees_overlays = true;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "desktop_dup_capture blue sensor=" << (w.capture_sensor_active ? 1 : 0)
      << " mismatch=" << w.capture_vs_present_mismatch
      << " deeper_than_streamproof_39=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::desktop_dup_capture
