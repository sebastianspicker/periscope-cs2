// Depth-track integration tests: drive shipped depth/* and server APIs on sim::World.
// No reimplementation of detectors; no always-pass stubs.

#include "depth/handle_multisample.hpp"
#include "depth/leakage_scorer.hpp"
#include "depth/multi_invariant_scorer.hpp"
#include "depth/trust_aggregator.hpp"
#include "depth/seller_fusion.hpp"
#include "depth/residual_research.hpp"
#include "sim/world.hpp"
#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"

#include <cstdio>
#include <string>

namespace {

int fails = 0;

void expect(bool c, const char* m) {
  if (!c) {
    std::fprintf(stderr, "FAIL: %s\n", m);
    ++fails;
  } else {
    std::printf("ok: %s\n", m);
  }
}

}  // namespace

int main() {
  // ── 1. Multi-sample handle race composition ──────────────────────────
  {
    auto w = sim::make_arena();
    depth::MultiSampleHandleDetector det;
    // Red race: open → hide → reopen (shipped helper).
    auto race = depth::run_handle_race(w, /*samples=*/3, /*syscall=*/false,
                                       /*claim_reputable=*/true,
                                       /*parent=*/0);
    expect(race.opened && race.hid && race.reopened, "handle race script steps");
    expect(race.read_ok, "handle race still RPM while enum-hidden");

    // Blue: ≥2 samples + composition (not a single bool).
    // Re-sample current world state twice with hide toggles already applied.
    det.push(det.sample(w, 0));
    // Force visibility flip for sample disagreement.
    for (auto& h : w.handles) {
      if (h.owner_pid == race.actor_pid) {
        h.hidden_during_enum = false;
      }
    }
    det.push(det.sample(w, 1));
    for (auto& h : w.handles) {
      if (h.owner_pid == race.actor_pid) {
        h.hidden_during_enum = true;
      }
    }
    det.push(det.sample(w, 2));
    auto ev = det.evaluate(w);
    expect(ev.samples_taken >= 2, "handle ≥2 samples");
    expect(ev.continuous_hit, "handle continuous truth hit");
    expect(ev.race_detected || ev.composed_hit, "handle race or composed hit");
    expect(ev.lineage_hit || ev.reputation_evasion, "lineage or rep composition");
    expect(ev.score >= 3.0, "handle composition score multi-signal");
    std::printf("  detail: %s\n", ev.detail.c_str());
  }

  // ── 2. Interest management + entity stream leakage scoring ───────────
  {
    auto w = sim::make_arena();
    depth::LeakageScorer scorer;
    server::Observer obs{{0, 0, 0}, 0};
    std::vector<depth::EntityTruth> all = {
        {1, {0, 0, 0}, 1, true, true},
        {2, {10, 0, 10}, 2, true, true},     // near enemy
        {3, {999, 0, 999}, 2, true, false},  // far
        {4, {800, 0, 800}, 2, true, false},  // far
        {5, {700, 0, 700}, 2, true, false},  // far
    };

    // Full replication: high leakage risk.
    scorer.set_fog(depth::FogPolicy::FullReplication);
    depth::StreamCryptoState open_stream{false, false, false, false};
    auto full = scorer.score(obs, 1, all, open_stream);
    expect(!full.structural_kill, "full repl not structural kill");
    expect(full.replicated_enemies >= 3, "full repl sends far enemies");

    // Partial leak fog: some far entities leak.
    scorer.set_fog(depth::FogPolicy::PartialLeak);
    scorer.set_partial_leak_fraction(0.5f);
    auto partial = scorer.score(obs, 1, all, open_stream);
    expect(partial.leakage_ratio > 0.0 || partial.leaked_beyond_fog > 0 ||
               partial.replicated_enemies > 1,
           "partial fog scores leakage");

    // Delayed origin still replicates far entities as delayed.
    scorer.set_fog(depth::FogPolicy::DelayedOrigin);
    auto delayed = scorer.score(obs, 1, all, open_stream);
    expect(delayed.delayed_origins > 0, "delayed origin count");

    // Red stream exfil then blue multi-step mitigate.
    auto ex = depth::run_stream_exfil_red(w);
    expect(ex.key_exfiltrated && ex.useful_radar, "stream exfil red");
    auto mit = scorer.mitigate_world(w, obs, 1, all);
    expect(w.entity_stream_encrypted && !w.client_has_stream_key,
           "mitigate strips client key");
    expect(!w.server_sends_full_enemy_origin, "mitigate enforces fog");
    expect(mit.structural_kill, "mitigate structural kill");
    std::printf("  leak detail: %s\n", mit.detail.c_str());
  }

  // ── 3. Multi-invariant info-advantage / overwatch ────────────────────
  {
    depth::MultiInvariantScorer sc;
    sc.set_fp_budget(1.5);
    sc.set_delay_frames(3);
    sc.set_overwatch_threshold(3.0);
    sc.set_ban_threshold(8.0);

    // Clean frames — FP budget absorbs noise.
    for (int i = 0; i < 2; ++i) {
      depth::RichDemoFrame f;
      f.t = i;
      f.has_vision_on_target = true;
      f.aim_on_hidden_target = false;
      sc.on_frame(f);
    }
    auto clean = sc.result();
    expect(clean.action == depth::DelayedAction::None ||
               clean.adjusted_score < 3.0,
           "clean frames low action");

    sc.reset();
    sc.set_fp_budget(1.5);
    // Multi-invariant hostile: vision + sound + latency + reports.
    for (int i = 0; i < 6; ++i) {
      depth::RichDemoFrame f;
      f.t = i;
      f.aim_on_hidden_target = true;
      f.has_vision_on_target = false;
      f.has_audio_on_target = false;
      f.rtt_ms = 80.0;
      f.peer_reports_on_actor = (i >= 2) ? 3 : 0;
      sc.on_frame(f);
    }
    auto bad = sc.result();
    expect(bad.vision_hits > 0 && bad.sound_hits > 0, "vision+sound invariants");
    expect(bad.latency_hits > 0 || bad.report_hits > 0,
           "latency or report invariant");
    int inv = (bad.vision_hits > 0) + (bad.sound_hits > 0) +
              (bad.latency_hits > 0) + (bad.report_hits > 0);
    expect(inv >= 2, "≥2 independent invariants");
    expect(bad.adjusted_score > 0, "FP budget applied (adj from raw)");
    expect(bad.threshold_crossed ||
               bad.action != depth::DelayedAction::None,
           "delayed threshold action");

    auto w = sim::make_arena();
    auto applied = depth::apply_overwatch_to_world(w, sc);
    expect(w.overwatch_score == applied.adjusted_score, "world overwatch_score");
    expect(w.overwatch_queued || applied.action == depth::DelayedAction::None ||
               applied.action == depth::DelayedAction::ObserveOnly,
           "overwatch queue path");
    std::printf("  scorer: %s\n", bad.detail.c_str());
  }

  // ── 4. HV trust aggregation over time ────────────────────────────────
  {
    auto w = sim::make_arena();
    depth::TrustAggregator agg;
    // Clean baseline
    auto clean = agg.evaluate_world_timeline(w, 3);
    expect(clean.allow_ranked, "clean trust allows ranked");
    expect(clean.action == depth::TrustAction::Allow, "clean allow");

    // Mild frozen world: 2 spoof families only — must SoftFlag, allow ranked.
    // Risk must NOT scale with evaluate_world_timeline tick count.
    auto mild = sim::make_arena();
    mild.trust.timing_spoofed = true;
    mild.trust.ept_hide_ac_pages = true;
    depth::TrustAggregator mild_agg;
    auto m1 = mild_agg.evaluate_world_timeline(mild, 1);
    auto m3 = mild_agg.evaluate_world_timeline(mild, 3);
    expect(m1.risk == m3.risk, "frozen mild risk stable across ticks");
    expect(m1.spoof_signals == m3.spoof_signals, "frozen spoof union stable");
    expect(m1.spoof_signals == 2, "mild has 2 spoof families (union)");
    expect(m1.policy_fails == 0, "mild has no policy fails");
    expect(m1.action == depth::TrustAction::SoftFlag, "mild SoftFlag not Block");
    expect(m1.allow_ranked, "mild still allows ranked");
    expect(m3.action == depth::TrustAction::SoftFlag, "ticks=3 still SoftFlag");
    expect(m3.allow_ranked, "ticks=3 still allows ranked");
    std::printf("  mild t1: %s\n  mild t3: %s\n", m1.detail.c_str(),
                m3.detail.c_str());

  // depth::plant_hostile_trust_timeline: depth: plant or apply educational residuals on World.
    depth::plant_hostile_trust_timeline(w);
    // Hostile constant samples (union — still multi-signal hard block)
    auto hostile = agg.evaluate_world_timeline(w, 3);
    expect(!hostile.allow_ranked, "hostile blocks ranked");
    expect(hostile.policy_fails + hostile.spoof_signals >= 2,
           "multiple trust signals");
    expect(hostile.risk >= 6.0, "aggregate risk multi-signal");

    // Timeline flips: start clean-looking then degrade (real push mutations).
    auto w2 = sim::make_arena();
    depth::TrustAggregator agg2;
    agg2.push(agg2.capture(w2, 0));
    w2.trust.vbs = false;
    w2.trust.hvci = false;
    w2.try_start_personal_hv("flip-hv");
    w2.trust.timing_spoofed = true;
    agg2.push(agg2.capture(w2, 1));
    w2.trust.attestation_valid = false;
    agg2.push(agg2.capture(w2, 2));
    auto flip = agg2.evaluate();
    expect(flip.inconsistent_pairs >= 1 || flip.spoof_signals >= 1,
           "timeline inconsistency");
    expect(!flip.allow_ranked || flip.action != depth::TrustAction::Allow,
           "flip blocks or flags");
    std::printf("  trust hostile: %s\n  flip: %s\n", hostile.detail.c_str(),
                flip.detail.c_str());
  }

  // ── 5. Ops seller/cluster fusion ≥3 families ─────────────────────────
  {
    auto w = sim::make_arena();
    depth::SellerFusionCorrelator fuse;
    auto empty = fuse.evaluate(w);
    expect(!empty.cluster_detected, "empty world no cluster");

    depth::SellerFusionCorrelator::plant_seller_cluster(w);
    auto hit = fuse.evaluate(w);
    expect(hit.families_hit >= 3, "≥3 signal families");
    expect(hit.cluster_detected, "cluster detected");
    expect(hit.risk > 0, "fusion risk > 0");
    expect(hit.action != depth::ClusterAction::None, "cluster action");
    std::printf("  seller: %s\n", hit.detail.c_str());
  }

  // ── 6. Residual research examples: VMX, BYOVD, DMA, SMM ──────────────
  {
    {
      auto w = sim::make_arena();
      auto r = depth::run_vmx_research_example(w);
      expect(r.red_achieved, "vmx red multi-step");
      expect(r.blue_detected, "vmx blue detect");
      expect(r.blue_mitigated, "vmx blue mitigate ranked");
    }
    {
      auto w = sim::make_arena();
      // Pre-condition: without residual, no block.
      expect(!w.byovd_policy_block && !w.ranked_access_denied,
             "byovd clean pre");
      auto r = depth::run_byovd_research_example(w);
      expect(r.red_achieved, "byovd red multi-step");
      expect(r.blue_detected, "byovd blue detect");
      expect(r.blue_mitigated, "byovd blue mitigate");
      expect(w.byovd_policy_block, "byovd_policy_block set");
      expect(w.ranked_access_denied, "ranked_access_denied set");
      // Post-mitigate: IOCTL must fail on shipped device_ioctl_read.
      const auto game = w.game_pid();
      auto ui = w.spawn("post-mitigate-probe.exe");
      std::vector<std::uint8_t> buf;
      bool after = w.device_ioctl_read(ui, "\\Device\\VulnCap", game,
                                       w.proc(game)->base, 4, buf);
      expect(!after, "post-mitigate ioctl denied on shipped path");
    }
    {
      auto w = sim::make_arena();
      auto r = depth::run_dma_research_example(w);
      expect(r.red_achieved, "dma red multi-step");
      expect(r.blue_detected, "dma blue detect");
      expect(r.blue_mitigated, "dma blue mitigate iommu+fog");
    }
    {
      auto w = sim::make_arena();
      auto r = depth::run_smm_research_example(w);
      expect(r.red_achieved, "smm red residual");
      expect(r.blue_detected, "smm blue detect");
      expect(r.blue_mitigated, "smm blue block ranked");
    }
    auto summary = depth::run_all_residual_research_examples();
    expect(summary.find("vmx") != std::string::npos, "residual summary vmx");
    expect(summary.find("smm") != std::string::npos, "residual summary smm");
    std::printf("  residuals:\n%s", summary.c_str());
  }

  // Classic server APIs still work (smoke).
  {
    server::InterestManager im;
    auto f = im.filter_for_client({{0, 0, 0}, 0}, 1,
                                  {{1, {0, 0, 0}, 1, true},
                                   {2, {999, 0, 999}, 2, true}},
                                  50.f);
    expect(f.size() == 1, "classic interest still filters");
    server::InfoAdvantageScorer ia;
    ia.on_frame({0, {}, 0, false, false, true});
    expect(ia.result().hits == 1, "classic info_advantage still scores");
  }

  if (fails) {
    std::fprintf(stderr, "depth_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("depth_tests: all passed\n");
  return 0;
}
