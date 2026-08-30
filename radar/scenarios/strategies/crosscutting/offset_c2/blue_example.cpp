// BLUE: offset/schema C2 net residual + schema-cache product + handle graph.

#include "blue_example.hpp"

#include <sstream>

namespace examples::offset_c2 {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;

  bool offset_net = false;
  for (const auto& n : w.net) {
    if (n.looks_like_offset_c2) {
      offset_net = true;
      break;
    }
  }
  if (offset_net) ++reasons;

  if (w.schema_remote_update || w.schema_fetch_count >= 1) ++reasons;
  if (w.schema_cache_active && !w.schema_cache_version.empty()) ++reasons;

  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      foreign_vm = true;
      break;
    }
  }
  if (foreign_vm) ++reasons;

  r.detected = reasons >= 2;
  r.mitigated = reasons >= 2;
  if (r.mitigated) {
    w.ranked_access_denied = true;
    w.schema_remote_update = false;
  }
  std::ostringstream oss;
  oss << "offset_c2 blue reasons=" << reasons
      << " offset_net=" << (offset_net ? 1 : 0)
      << " schema_remote=" << (w.schema_fetch_count)
      << " handle=" << (foreign_vm ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::offset_c2
