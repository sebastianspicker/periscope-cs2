// T3 Duel — policy, HV, bridge, fallbacks, server residual + PlatformAc full.

#include "ac/telemetry.hpp"
#include "server/ban_correlator.hpp"
#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include "t0_red/cheat_client.hpp"
#include "t2_red/kernel_radar.hpp"
#include "t3_blue/platform_ac.hpp"
#include "t3_red/hv_radar.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "T3: personal HV + bridge. Blue wins with trust policy + residual server.");

  auto world = sim::make_arena();
  ac::MemoryTelemetrySink sink;
  t3_blue::PlatformAc blue(world, sink);

  n.move(sim::Side::Red, "Try personal HV with VBS/HVCI still on",
         "Common fail: user didn't follow README yet.");
  t3_red::HvRadar red(world);
  const bool hv_blocked = !red.try_hv();
  n.say(sim::Side::System, hv_blocked ? "HV denied by VBS/HVCI" : "HV started");

  n.counter(sim::Side::Blue, "Ranked trust policy",
            "Competitive requires VBS+HVCI — deny unlocked playground.");
  auto pol = blue.evaluate_ranked();
  std::printf("    ranked_allowed=%d (still default trust)\n",
              pol.ranked_allowed ? 1 : 0);

  n.move(sim::Side::Red, "Full multi-step HV loop (trust off → HV → bridge → stealth)",
         "Social engineering the security boundary + residual scars.");
  auto loop = red.run_full_loop("ACLABHV", true);
  n.say(sim::Side::Red, loop.detail);
  std::printf(
      "    full_loop trust_cleared=%d hv=%d bridge=%d entities=%d "
      "ept=%d timing=%d attest_fail=%d no_handle=%d vendor=%s\n",
      loop.trust_cleared ? 1 : 0, loop.hv_started ? 1 : 0,
      loop.bridge_open ? 1 : 0, loop.entity_count, loop.ept_hide ? 1 : 0,
      loop.timing_spoof ? 1 : 0, loop.attest_fail ? 1 : 0,
      loop.no_game_handle ? 1 : 0, loop.vendor.c_str());
  std::printf("    entities=%zu read_ops=%d bytes=%llu\n", red.entities().size(),
              loop.read_ops,
              static_cast<unsigned long long>(loop.bytes_read));

  n.counter(sim::Side::Blue, "Full platform multi-sensor scan",
            "Policy + HV probe + bridge + attest + dual-view + aggregate + scars.");
  auto det = blue.full();
  for (const auto& r : det.reasons) {
    n.say(sim::Side::Blue, r);
  }
  std::printf(
      "    ranked_allowed=%d hv=%d bridge=%d policy=%d attest=%d dual=%d "
      "sk_dirty=%d agg_block=%d risk=%.1f\n",
      det.ranked_allowed ? 1 : 0, det.hv_anomaly ? 1 : 0,
      det.bridge_hit ? 1 : 0, det.policy_fail ? 1 : 0, det.attest_fail ? 1 : 0,
      det.ept_dual_view ? 1 : 0, det.sk_dirty ? 1 : 0,
      det.trust_aggregate_block ? 1 : 0, det.risk);
  if (!det.detail.empty()) {
    std::printf("    detail=%s\n", det.detail.c_str());
  }

  n.move(sim::Side::Red, "If HV fails mid-session → T2/T0",
         "Real packs ship fallbacks; blue must keep lower detectors live.");
  t2_red::KernelRadar t2(world, "fallback-ud.exe");
  t2.bring_up(t2_red::KernelPath::CustomDriver);
  t0_red::CheatClient t0(world, "fallback-rpm.exe");
  t0.attach_to_game();
  n.say(sim::Side::Lesson,
        "Now world has HV bridge + memrw device + RPM handle — multi-signal.");

  n.counter(sim::Side::Blue, "Server interest management",
            "Even perfect client reads die if data was never sent.");
  server::InterestManager im;
  std::vector<server::WorldEntity> all = {
      {0, {0, 0, 0}, 1, true},
      {1, {500, 0, 500}, 2, true},
  };
  auto vis = im.filter_for_client({{0, 0, 0}, 0}, 1, all, 50.f);
  std::printf("    replicated_entities=%zu (far enemy culled)\n", vis.size());

  n.counter(sim::Side::Blue, "Info-advantage scoring",
            "Human radar use still leaks impossible knowledge.");
  server::InfoAdvantageScorer ia;
  for (int i = 0; i < 4; ++i) {
    ia.on_frame({double(i), {}, 90.f, false, false, true});
  }
  server::BanCorrelator ban;
  ac::RiskAggregator risk;
  for (const auto& e : sink.events()) {
    risk.ingest(e);
  }
  auto decision = ban.evaluate(risk.state(), ia.result().score);
  std::printf("    ban_action reason=%s score_ia=%.1f\n", decision.reason,
              ia.result().score);

  const bool blue_wins = !det.ranked_allowed || det.hv_anomaly ||
                         det.bridge_hit || det.attest_fail ||
                         det.trust_aggregate_block || det.risk > 0 ||
                         !det.reasons.empty();
  n.result(blue_wins,
           "T3 constrained by policy/probe/bridge/attest/aggregate; server residual.");

  n.say(sim::Side::Lesson,
        "Curriculum complete. Re-read t*/learn/LESSON.md and walk code in duel_t*.");
  return blue_wins ? 0 : 1;
}
