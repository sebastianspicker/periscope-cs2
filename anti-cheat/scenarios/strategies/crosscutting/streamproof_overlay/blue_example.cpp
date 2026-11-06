// BLUE: multi-reason present vs capture streamproof residual (signals >= 2).

#include "blue_example.hpp"

#include <sstream>

namespace examples::streamproof_overlay {

BlueResult detect(sim::World& w) {
  BlueResult r;
  bool sp = false;
  bool band4 = false;
  for (const auto& o : w.overlays) {
    if (o.stream_proof) sp = true;
    if (o.band4_zorder) band4 = true;
  }
  r.present_overlay = !w.overlays.empty() && (sp || band4);

  // Reason 1: stream-proof flag on present-path overlay.
  if (sp) {
    r.reasons.emplace_back("stream_proof overlay on present path");
  }

  // Reason 2: elevated band-4 Z-order residual.
  if (band4) {
    r.reasons.emplace_back("band4_zorder overlay residual");
  }

  // Reason 3: capture path empty while present path has overlay.
  r.capture_miss = r.present_overlay && !w.capture_sees_overlays;
  if (r.capture_miss) {
    r.reasons.emplace_back("capture_sees_overlays false with present overlay");
  }

  // Reason 4: capture sensor active yet still misses streamproof pixels.
  if ((w.desktop_duplication || w.desktop_duplication_active) && r.capture_miss) {
    r.reasons.emplace_back("desktop_duplication active but capture miss");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.capture_sees_overlays = true;  // require capture parity
    w.ranked_access_denied = true;
  }

  std::ostringstream oss;
  oss << "streamproof_overlay blue signals=" << r.signals
      << " present=" << (r.present_overlay ? 1 : 0)
      << " capture_miss=" << (r.capture_miss ? 1 : 0)
      << " sp=" << (sp ? 1 : 0) << " band4=" << (band4 ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::streamproof_overlay
