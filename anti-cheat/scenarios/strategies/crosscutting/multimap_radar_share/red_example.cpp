#include "red_example.hpp"
#include <sstream>
namespace examples::multimap_radar_share {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("webradar-share.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  w.add_net(sim::NetFlow{r.actor_pid, "radar-saas.example:443", false, true});
  w.multimap_radar_share = true;
  w.radar_maps_shared = 3;  // dust2, mirage, inferno class residual
  w.radar_share_ux_active = true;
  w.radar_share_token = "share-tok-lab-9f3a";
  // No local overlay — phone/browser multi-map share is the UI.
  r.achieved = w.multimap_radar_share && w.radar_maps_shared >= 2 &&
               w.radar_share_ux_active && !w.radar_share_token.empty();
  std::ostringstream oss;
  oss << "multimap_radar_share red maps=" << w.radar_maps_shared
      << " share_ux=1 token_len=" << w.radar_share_token.size()
      << " (beyond plain web_phone_radar)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::multimap_radar_share
