// Optional narrated lab: representative xc ops pairs on sim::World.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "strategies/crosscutting/account_graph/red_example.hpp"
#include "strategies/crosscutting/account_graph/blue_example.hpp"
#include "strategies/crosscutting/report_velocity/red_example.hpp"
#include "strategies/crosscutting/report_velocity/blue_example.hpp"
#include "strategies/crosscutting/vpn_proxy_graph/red_example.hpp"
#include "strategies/crosscutting/vpn_proxy_graph/blue_example.hpp"
#include "strategies/crosscutting/network_c2_intel/red_example.hpp"
#include "strategies/crosscutting/network_c2_intel/blue_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/red_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/blue_example.hpp"
#include "strategies/crosscutting/overwatch_multisignal/red_example.hpp"
#include "strategies/crosscutting/overwatch_multisignal/blue_example.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "Ops lab — multi-step red ops scars vs multi-reason blue on sim::World.");

  int blue_hits = 0;
  int rounds = 0;

  // 1. Account graph / seller fusion
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "account_graph",
           "Plant multi-family seller cluster (accounts + VPN + C2 + build).");
    auto rr = examples::account_graph::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[account] accounts=%zu vpn=%d achieved=%d\n", w.accounts.size(),
                w.vpn_proxy_active ? 1 : 0, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "seller fusion",
              "Link HWID/payment pairs; fuse VPN + C2 + reports.");
    auto br = examples::account_graph::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 2. Report velocity farm
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "report_velocity",
           "Smurf farm: multi accounts, shared payment, report storm.");
    auto rr = examples::report_velocity::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[report] accounts=%d report_sum=%d achieved=%d\n",
                rr.achieved, rr.achieved, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "payment cluster",
              "Per-account threshold or payment-cluster report sum.");
    auto br = examples::report_velocity::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 3. VPN / proxy graph
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "vpn_proxy_graph",
           "VPN active + clustered accounts on same vpn_* ip_class.");
    auto rr = examples::vpn_proxy_graph::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[vpn] vpn=%d clustered=%d achieved=%d\n", rr.achieved ? 1 : 0,
                rr.achieved, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "vpn + ip_cluster",
              "vpn_proxy_active and/or multi-account vpn_* class.");
    auto br = examples::vpn_proxy_graph::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 4. Network C2 intel
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "network_c2_intel",
           "Loader process with auth / offset CDN / radar SaaS flows.");
    auto rr = examples::network_c2_intel::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[c2] loader=%u net=%zu achieved=%d\n", rr.achieved,
                w.net.size(), rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "domain intel",
              "Flag offset_c2 / radar_saas flows (pair with client scars).");
    auto br = examples::network_c2_intel::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 5. AC self-integrity tamper
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "ac_self_integrity",
           "Patch AC module text_hash so sensors go silent/lie.");
    auto rr = examples::ac_self_integrity::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[ac_int] ac_pid=%u achieved=%d\n", rr.achieved,
                rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "dirty modules",
              "Any AC module with text_hash != clean.");
    auto br = examples::ac_self_integrity::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 6. Overwatch multi-signal scoring
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "overwatch_multisignal",
           "Mild handle + VPN + unique build; hope score stays delayed.");
    auto rr = examples::overwatch_multisignal::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[ow_ms] achieved=%d\n", rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "multi-signal score",
              "Aggregate handle+vpn+poly; queue when score >= 2.");
    auto br = examples::overwatch_multisignal::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  const bool all = blue_hits == rounds;
  n.result(all, all ? "All blue multi-reason detectors fired."
                    : "Some blue detectors missed (lesson bug).");
  std::printf("ops_lab: blue_hits=%d/%d\n", blue_hits, rounds);
  return all ? 0 : 1;
}
