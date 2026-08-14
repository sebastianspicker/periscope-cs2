#include "blue_example.hpp"
#include <sstream>
namespace examples::noscope_inaccuracy_viz {
BlueResult detect(sim::World& w) {
  BlueResult r;
  r.detected = w.noscope_inaccuracy_viz && w.noscope_spread_shown > 0.1f;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.noscope_inaccuracy_viz = false;
    w.noscope_spread_shown = 0.f;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "noscope_inaccuracy_viz blue detected=" << (r.detected ? 1 : 0)
      << " combat_presentation=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::noscope_inaccuracy_viz
