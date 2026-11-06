#include "red_example.hpp"
namespace examples::silent_aim_desync {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("silent-aim.exe");
  w.silent_aim_active = true;
  for (int i = 0; i < 5; ++i) {
    sim::AimSample s;
    s.camera_yaw = 10.f;
    s.camera_pitch = 0.f;
    s.server_aim_yaw = 45.f + static_cast<float>(i);  // desync
    s.server_aim_pitch = 5.f;
    s.challenge_passed = false;
    w.aim_samples.push_back(s);
  }
  r.achieved = w.silent_aim_active && w.aim_samples.size() >= 4;
  r.detail = "silent_aim_desync red samples=" + std::to_string(w.aim_samples.size()) + " challenge_fail";
  w.note(r.detail);
  return r;
}
}  // namespace examples::silent_aim_desync
