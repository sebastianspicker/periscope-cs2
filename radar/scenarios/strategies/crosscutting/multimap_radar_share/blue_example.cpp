#include "blue_example.hpp"
#include <sstream>
namespace examples::multimap_radar_share {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.multimap_radar_share) ++reasons;
  if (w.radar_maps_shared >= 2) ++reasons;
  if (w.radar_share_ux_active && !w.radar_share_token.empty()) ++reasons;
  bool saas = false;
  for (const auto& n : w.net) {
    if (n.looks_like_radar_saas) {
      saas = true;
      break;
    }
  }
  if (saas) ++reasons;
  bool handle = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) {
        handle = true;
        break;
      }
    }
  }
  if (handle) ++reasons;
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 3;
  if (r.mitigated) {
    w.multimap_radar_share = false;
    w.radar_share_ux_active = false;
    w.radar_share_token.clear();
    w.radar_maps_shared = 0;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "multimap_radar_share blue reasons=" << reasons
      << " maps=" << w.radar_maps_shared << " saas=" << (saas ? 1 : 0)
      << " not_map_packs=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::multimap_radar_share
