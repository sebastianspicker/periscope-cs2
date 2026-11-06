// BLUE: |expected - applied| tiny across spray samples = RCS assist residual.

#include "blue_example.hpp"

#include <cmath>
#include <sstream>

namespace examples::rcs_pattern {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int fit = 0;
  float max_err = 0.f;
  for (const auto& s : w.aim_samples) {
    if (s.recoil_expected_pitch <= 0.f) continue;
    const float err = std::fabs(s.recoil_expected_pitch - s.rcs_applied_pitch);
    if (err > max_err) max_err = err;
    if (err < 0.05f) ++fit;
  }
  r.fit_samples = fit;
  r.max_err = max_err;
  r.detected = w.rcs_active && fit >= 6 && max_err < 0.05f;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.rcs_active = false;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "rcs_pattern blue fit=" << r.fit_samples
      << " max_err=" << r.max_err
      << " rcs_flag=" << (w.rcs_active ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::rcs_pattern
