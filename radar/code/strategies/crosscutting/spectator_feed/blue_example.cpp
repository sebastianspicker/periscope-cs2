// BLUE: detect spectator feed + delayed origin; cut delayed intel.

#include "blue_example.hpp"

#include <sstream>

namespace examples::spectator_feed {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.feed = w.spectator_feed_active && w.spectator_count >= 1;
  r.delayed_origin = w.spectator_has_delayed_enemy_origin;
  r.detected = r.feed && r.delayed_origin;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.spectator_has_delayed_enemy_origin = false;
    w.spectator_feed_active = false;
    w.spectator_count = 0;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "spectator_feed blue feed=" << (r.feed ? 1 : 0)
      << " delayed=" << (r.delayed_origin ? 1 : 0)
      << " count=" << w.spectator_count;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::spectator_feed
