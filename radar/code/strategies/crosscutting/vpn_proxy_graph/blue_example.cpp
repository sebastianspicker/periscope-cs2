#include "blue_example.hpp"
namespace examples::vpn_proxy_graph {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int c = 0;
  for (const auto& a : w.accounts) if (a.ip_class.rfind("vpn_",0)==0) ++c;
  r.detected = w.vpn_proxy_active && c >= 2;
  if (r.detected) { r.mitigated = true; w.overwatch_queued = true; }
  r.detail = "vpn_proxy_graph blue vpn_cluster=" + std::to_string(c);
  w.note(r.detail);
  return r;
}
}  // namespace examples::vpn_proxy_graph
