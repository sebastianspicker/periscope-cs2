// T3 BLUE prototype — PlatformAc on World scarred by HvRadar + residual server.

#include "ac/proto_log.hpp"
#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "server/ban_correlator.hpp"
#include "server/info_advantage.hpp"
#include "sim/world.hpp"
#include "t3_blue/attestation_gate.hpp"
#include "t3_blue/bridge_intel.hpp"
#include "t3_blue/hv_probe.hpp"
#include "t3_blue/platform_ac.hpp"
#include "t3_blue/trust_policy.hpp"
#include "t3_red/hv_radar.hpp"

#include <cstdio>

int main() {
  ac::proto_banner("BLUE", "T3", "PlatformAc on HV-scarred World + residual");

  ac::MemoryTelemetrySink sink;
  ac::RiskAggregator risk;

  // Prefer World path: red scars trust/HV/bridge/stealth, blue evaluates PlatformAc.
  auto world = sim::make_arena();
  t3_red::HvRadar red(world);
  auto scar = red.run_full_loop("ACLABHV", /*stealth=*/true);
  ac::proto_kv("red_hv", scar.hv_started);
  ac::proto_kv("red_bridge", scar.bridge_open);
  ac::proto_kv("red_entities", static_cast<std::size_t>(scar.entity_count));
  ac::proto_kv("red_no_handle", scar.no_game_handle);
  ac::proto_kv("red_ept", scar.ept_hide);
  ac::proto_kv("red_timing", scar.timing_spoof);
  ac::proto_kv("red_attest_fail", scar.attest_fail);

  t3_blue::PlatformAc platform(world, sink);
  auto full = platform.full();
  ac::proto_kv("platform_ranked", full.ranked_allowed);
  ac::proto_kv("platform_policy_fail", full.policy_fail);
  ac::proto_kv("platform_hv", full.hv_anomaly);
  ac::proto_kv("platform_bridge", full.bridge_hit);
  ac::proto_kv("platform_attest_fail", full.attest_fail);
  ac::proto_kv("platform_dual_view", full.ept_dual_view);
  ac::proto_kv("platform_sk_dirty", full.sk_dirty);
  ac::proto_kv("platform_agg_block", full.trust_aggregate_block);
  ac::proto_kv("platform_risk", full.risk);
  ac::proto_line("platform_detail", full.detail);
  for (const auto& reason : full.reasons) {
    ac::proto_line("platform_reason", reason);
  }

  // TrustPolicy / AttestationGate also consume World-derived host state.
  t3_blue::TrustPolicy policy(sink);
  policy.require_vbs(true);
  policy.require_hvci(true);
  auto ranked = policy.evaluate_world(world);
  ac::proto_kv("policy_allow_ranked", ranked.allow_ranked);
  ac::proto_line("policy_reason", ranked.reason);

  t3_blue::AttestationGate gate(policy);
  auto world_attest = gate.check_world(world);
  ac::proto_kv("world_attest_allow", world_attest.allow_ranked);
  ac::proto_line("world_attest_reason", world_attest.reason);

  t3_blue::BridgeIntel intel(sink);
  intel.seed_lab_indicators();
  auto bridge_scan = intel.scan_world(world);
  ac::proto_kv("bridge_world_hit", bridge_scan.hit);
  ac::proto_kv("bridge_world_risk", bridge_scan.risk);

  t3_blue::HvProbe probe(sink);
  auto hv = probe.analyze_world(world);
  ac::proto_kv("hv_probe_anomaly", hv.anomaly);

  // Residual: info-advantage even if client signals were weak.
  server::InfoAdvantageScorer ia;
  ia.on_frame({1.0, {}, 90.f, false, false, true});
  ia.on_frame({1.1, {}, 91.f, false, false, true});
  ia.on_frame({1.2, {}, 92.f, false, false, true});

  for (const auto& ev : sink.events()) {
    risk.ingest(ev);
  }
  server::BanCorrelator ban;
  auto decision = ban.evaluate(risk.state(), ia.result().score);

  ac::proto_events(sink);
  ac::proto_kv("risk_score", risk.state().score);
  ac::proto_kv("info_advantage", ia.result().score);
  ac::proto_line("ban_action",
                 decision.action == server::BanAction::None            ? "none"
                 : decision.action == server::BanAction::FlagOverwatch ? "overwatch"
                 : decision.action == server::BanAction::SoftRestrict  ? "soft_restrict"
                                                                       : "delayed_ban");
  ac::proto_line("ban_reason", decision.reason);

  const bool detected = !full.ranked_allowed || full.policy_fail ||
                        full.hv_anomaly || full.bridge_hit ||
                        full.attest_fail || full.ept_dual_view ||
                        full.trust_aggregate_block || full.risk > 0 ||
                        !ranked.allow_ranked || risk.state().score > 0;
  ac::proto_kv("T3_DETECTED", detected);
  std::puts(detected ? "prototype ok (t3 surface constrained/caught)"
                     : "prototype FAIL");
  return detected ? 0 : 1;
}
