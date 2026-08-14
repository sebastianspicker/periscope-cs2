#include "red_example.hpp"

namespace examples::report_velocity {

RedResult apply(sim::World& w) {
  RedResult r;

  // Phase 1: spawn report-farm orchestrator.
  r.actor_pid = w.spawn("report-farm.exe");
  ++r.steps;

  // Phase 2: seed high-velocity report accounts on shared payment fingerprint.
  w.add_account(sim::Account{"r1", "h1", "pay_v", "ip1", 40});
  w.add_account(sim::Account{"r2", "h2", "pay_v", "ip1", 35});
  ++r.steps;

  // Phase 3: expand cluster with second IP class + clean decoy.
  w.add_account(sim::Account{"r3", "h3", "pay_v", "ip2", 30});
  w.add_account(sim::Account{"clean", "hc", "pay_other", "ip3", 0});
  ++r.steps;

  // Phase 4: optional network residual for farm C2-style coordination.
  w.add_net({r.actor_pid, "lab.report-farm.invalid:443", false, false});
  w.vpn_proxy_active = true;
  ++r.steps;

  for (const auto& a : w.accounts) r.report_sum += a.reports;
  r.achieved = r.report_sum >= 50 && w.accounts.size() >= 4 && r.steps >= 2;
  r.detail = "report_velocity red steps=" + std::to_string(r.steps) +
             " report_sum=" + std::to_string(r.report_sum) +
             " accounts=" + std::to_string(w.accounts.size()) +
             " vpn=" + std::to_string(w.vpn_proxy_active ? 1 : 0);
  w.note(r.detail);
  return r;
}

}  // namespace examples::report_velocity
