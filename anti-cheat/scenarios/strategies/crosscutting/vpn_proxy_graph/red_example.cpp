#include "red_example.hpp"
namespace examples::vpn_proxy_graph {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("vpn-radar.exe");
  w.vpn_proxy_active = true;
  w.add_account(sim::Account{"v1","h","pay","vpn_exit_9",1});
  w.add_account(sim::Account{"v2","h2","pay","vpn_exit_9",1});
  w.add_account(sim::Account{"v3","h3","pay2","vpn_exit_9",0});
  r.cluster = 0;
  for (const auto& a : w.accounts) if (a.ip_class.find("vpn_") == 0) ++r.cluster;
  r.achieved = w.vpn_proxy_active && r.cluster >= 2;
  r.detail = "vpn_proxy_graph red vpn=1 cluster=" + std::to_string(r.cluster);
  w.note(r.detail);
  return r;
}
}  // namespace examples::vpn_proxy_graph
