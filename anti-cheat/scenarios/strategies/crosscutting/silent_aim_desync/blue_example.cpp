#include "blue_example.hpp"
namespace examples::silent_aim_desync {
BlueResult detect(sim::World& w) {
  BlueResult r;
  r.detected = w.silent_aim_active && w.aim_samples.size() >= 2;
  if (r.detected) { r.mitigated = true; w.overwatch_queued = true; }
  r.detail = "silent_aim_desync blue active=" + std::to_string(w.silent_aim_active?1:0) +
             " samples=" + std::to_string(w.aim_samples.size());
  w.note(r.detail);
  return r;
}
}  // namespace examples::silent_aim_desync
