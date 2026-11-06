// RED: applied pitch correction tracks expected spray pattern almost perfectly.

#include "red_example.hpp"

#include <cmath>
#include <sstream>

namespace examples::rcs_pattern {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("rcs-assist.exe");
  // Spray phase 1..N: expected pitch grows; RCS counters almost exactly.
  for (int i = 1; i <= 10; ++i) {
    sim::AimSample s;
    s.recoil_expected_pitch = 0.35f * static_cast<float>(i);
    s.rcs_applied_pitch = s.recoil_expected_pitch - 0.01f;  // near-perfect
    s.camera_pitch = -s.rcs_applied_pitch;
    w.aim_samples.push_back(s);
  }
  r.samples = static_cast<int>(w.aim_samples.size());
  w.rcs_active = true;
  w.rcs_pattern_samples = r.samples;

  r.achieved = w.rcs_active && w.rcs_pattern_samples >= 8;
  std::ostringstream oss;
  oss << "rcs_pattern red samples=" << r.samples << " near_perfect_fit=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::rcs_pattern
