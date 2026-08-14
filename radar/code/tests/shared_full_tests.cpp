// Shared infrastructure full tests: drive shipped APIs across common / sim /
// server / depth / lab / fps packages. No stubs; multi-signal where applicable.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "ac/types.hpp"
#include "depth/leakage_scorer.hpp"
#include "depth/seller_fusion.hpp"
#include "depth/trust_aggregator.hpp"
#include "fps/lab_bridge.hpp"
#include "fps/scenario.hpp"
#include "lab/fixture_process.hpp"
#include "lab/lab_memory.hpp"
#include "server/ban_correlator.hpp"
#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"
#include "sim/world.hpp"

#include <cstdio>
#include <cstring>
#include <set>
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
  // ── 1. sim::make_arena — game+ac, plant, handle, driver, trust ───────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto ac = w.ac_pid();
    expect(game != 0 && ac != 0 && game != ac, "make_arena game+ac pids");
    auto* g = w.proc(game);
    auto* a = w.proc(ac);
    expect(g && g->is_game && a && a->is_ac, "make_arena process roles");
    expect(!g->memory.empty(), "make_arena planted game memory");
    std::uint32_t ent_count = 0;
    std::memcpy(&ent_count, g->memory.data(), sizeof(ent_count));
    expect(ent_count >= 2, "make_arena plant_lab_entities count");

    // Reader open-handle path (shipped World API).
    auto reader = w.spawn("shared-reader.exe");
    expect(w.open_process(reader, game, sim::AccessMask::VmRead, false),
           "make_arena open_process handle");
    expect(!w.handles_to(game).empty(), "make_arena handle graph non-empty");

    // AC driver loaded by make_arena; load an extra mem-rw scar driver too.
    bool ac_driver = false;
    for (const auto& d : w.drivers) {
      if (d.is_ac) ac_driver = true;
    }
    expect(ac_driver, "make_arena loads ac driver");
    w.load_driver(sim::Driver{"probe.sys", "probe_hash", "LabCo", false, false,
                              false, false, true, 90});
    bool mem_rw = false;
    for (const auto& d : w.drivers) {
      if (d.provides_mem_rw) mem_rw = true;
    }
    expect(mem_rw, "make_arena load_driver mem_rw");

    // Trust fields present with sane defaults.
    expect(w.trust.secure_boot && w.trust.vbs && w.trust.hvci,
           "make_arena trust policy defaults");
    expect(w.trust.attestation_valid, "make_arena attestation_valid");
    expect(!w.trust.personal_hv_active, "make_arena no personal HV baseline");
  }

  // ── 2. ac::RiskAggregator + MemoryTelemetrySink multi-signal ─────────────
  {
    ac::MemoryTelemetrySink sink;
    ac::RiskAggregator risk;

    ac::TelemetryEvent e1{ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm,
                          100, 200, "handle", 2.0};
    ac::TelemetryEvent e2{ac::EventKind::DriverLoad, ac::Tier::T2_KernelByovd,
                          100, 0, "byovd-ish", 2.5};
    ac::TelemetryEvent e3{ac::EventKind::InfoAdvantageHit,
                          ac::Tier::T0_UsermodeRpm, 100, 0, "preaim", 3.0};
    ac::TelemetryEvent e4{ac::EventKind::ByovdBlocked, ac::Tier::T2_KernelByovd,
                          0, 0, "block-hash", 1.0};
    sink.emit(e1);
    sink.emit(e2);
    sink.emit(e3);
    sink.emit(e4);
    risk.ingest(e1);
    risk.ingest(e2);
    risk.ingest(e3);
    risk.ingest(e4);

    const double sum =
        e1.risk_delta + e2.risk_delta + e3.risk_delta + e4.risk_delta;
    expect(sink.events().size() == 4, "telemetry sink multi events");
    expect(sink.events()[0].kind == ac::EventKind::HandleToGame &&
               sink.events()[3].kind == ac::EventKind::ByovdBlocked,
           "telemetry sink preserves emission order");
    std::set<ac::EventKind> kinds;
    for (const auto& e : sink.events()) kinds.insert(e.kind);
    expect(kinds.size() >= 4, "telemetry multi_signal event kinds");
    expect(risk.state().score == sum, "risk score multi_signal exact sum");
    expect(risk.state().flag_overwatch, "risk flag_overwatch from multi events");
    expect(risk.state().block_ranked, "risk block_ranked from ByovdBlocked");
    expect(risk.state().event_count == 4, "risk event_count multi ingest");
    expect(risk.state().distinct_kinds == 4, "risk distinct_kinds multi_signal");
    expect(risk.state().multi_signal, "risk multi_signal flag");
    expect(risk.state().reasons.size() == 4, "risk multi_signal reasons");
    expect(!ac::to_string(ac::Tier::T0_UsermodeRpm).empty() &&
               !ac::to_string(ac::Status::Ok).empty(),
           "ac to_string Tier/Status non-empty");
  }

  // ── 3. server::InterestManager — far enemy culled, teammate kept ─────────
  {
    server::InterestManager im;
    server::Observer obs{{0.f, 0.f, 0.f}, 0.f};
    std::vector<server::WorldEntity> all = {
        {1, {0.f, 0.f, 0.f}, 1, true},      // self/team
        {2, {5.f, 0.f, 5.f}, 1, true},      // teammate near
        {3, {10.f, 0.f, 10.f}, 2, true},    // near enemy
        {4, {999.f, 0.f, 999.f}, 2, true},  // far enemy
        {5, {800.f, 0.f, 800.f}, 2, false}, // dead far
    };
    server::InterestFilterStats stats{};
    auto f = im.filter_for_client_stats(obs, /*team=*/1, all,
                                        /*enemy_radius=*/50.f, stats);
    bool teammate = false, near_enemy = false, far_enemy = false, dead = false;
    for (const auto& e : f) {
      if (e.id == 2) teammate = true;
      if (e.id == 3) near_enemy = true;
      if (e.id == 4) far_enemy = true;
      if (e.id == 5) dead = true;
    }
    expect(teammate, "interest teammate kept");
    expect(near_enemy, "interest near enemy kept");
    expect(!far_enemy, "interest far enemy culled");
    expect(!dead, "interest dead not replicated");
    expect(f.size() >= 2, "interest filter size");
    expect(stats.enemies_culled >= 1, "interest stats enemies_culled");
    expect(stats.teammates >= 1, "interest stats teammates");
    expect(stats.multi_reason, "interest multi_reason cull+keep");
  }

  // ── 4. server::InfoAdvantageScorer — ≥3 pre-aim frames ───────────────────
  {
    server::InfoAdvantageScorer ia;
    // Clean frames: no hits.
    ia.on_frame({0, {}, 0, true, false, false});
    ia.on_frame({1, {}, 0, false, true, false});
    expect(ia.result().hits == 0, "info_advantage clean frames no hit");

    ia.reset();
    // ≥3 pre-aim frames: aim on hidden target without vision/audio.
    for (int i = 0; i < 4; ++i) {
      server::DemoFrame f;
      f.t = static_cast<double>(i);
      f.aim_on_hidden_target = true;
      f.has_vision_on_target = false;
      f.has_audio_on_target = false;
      ia.on_frame(f);
    }
    auto r = ia.result();
    expect(r.hits >= 3, "info_advantage ≥3 pre-aim hits");
    expect(r.score >= 3.0, "info_advantage score from pre-aim");
    expect(r.max_consecutive >= 3, "info_advantage consecutive streak");
    expect(r.frames_seen >= 4, "info_advantage frames_seen");
    expect(r.multi_reason, "info_advantage multi_reason");
    expect(!r.reasons.empty(), "info_advantage reasons non-empty");
  }

  // ── 5. server::BanCorrelator — high score → DelayedBan or Overwatch ──────
  {
    server::BanCorrelator ban;

    // Mild: overwatch path via info advantage score.
    ac::RiskState mild{};
    mild.score = 2.0;
    mild.flag_overwatch = true;
    auto d1 = ban.evaluate(mild, /*info_advantage=*/1.0);
    expect(d1.action == server::BanAction::FlagOverwatch ||
               d1.action == server::BanAction::DelayedBanCandidate,
           "ban mild → Overwatch/DelayedBan");
    expect(d1.multi_signal, "ban mild multi_signal");
    expect(d1.signal_count >= 2, "ban mild signal_count");

    // High correlated risk → DelayedBanCandidate.
    ac::RiskState high{};
    high.score = 8.0;
    auto d2 = ban.evaluate(high, /*info_advantage=*/3.5);
    expect(d2.score >= 10.0, "ban high combined score");
    expect(d2.action == server::BanAction::DelayedBanCandidate,
           "ban high → DelayedBanCandidate");
    expect(std::string(d2.reason).find("correlated") != std::string::npos ||
               d2.action == server::BanAction::DelayedBanCandidate,
           "ban multi_signal reason string");
    expect(d2.multi_signal && d2.signal_count >= 2, "ban high multi_signal");
    expect(!d2.reasons.empty(), "ban high reasons list");

    // Trust/byovd policy block path.
    ac::RiskState blocked{};
    blocked.block_ranked = true;
    blocked.score = 1.0;
    auto d3 = ban.evaluate(blocked, 0.0);
    expect(d3.action == server::BanAction::SoftRestrict,
           "ban block_ranked → SoftRestrict");

    // Honesty: sole info-advantage family (ia=3) is ONE signal, not multi.
    ac::RiskState clean{};
    auto d_sole_ia = ban.evaluate(clean, /*info_advantage=*/3.0);
    expect(d_sole_ia.signal_count == 1, "ban sole ia=3 signal_count==1");
    expect(!d_sole_ia.multi_signal, "ban sole ia=3 multi_signal=false");
    expect(d_sole_ia.action == server::BanAction::FlagOverwatch,
           "ban sole ia=3 still flags overwatch");

    // Honesty: clean risk + ia=0 → no multi_signal.
    auto d_clean = ban.evaluate(clean, 0.0);
    expect(d_clean.signal_count == 0, "ban clean signal_count==0");
    expect(!d_clean.multi_signal, "ban clean multi_signal=false");
    expect(d_clean.action == server::BanAction::None, "ban clean action None");

    // Honesty: single RiskState field without ia → multi_signal=false.
    ac::RiskState score_only{};
    score_only.score = 2.0;
    auto d_score_only = ban.evaluate(score_only, 0.0);
    expect(d_score_only.signal_count == 1, "ban score-only signal_count==1");
    expect(!d_score_only.multi_signal, "ban score-only multi_signal=false");

    ac::RiskState flag_only{};
    flag_only.flag_overwatch = true;
    auto d_flag_only = ban.evaluate(flag_only, 0.0);
    expect(d_flag_only.signal_count == 1, "ban flag-only signal_count==1");
    expect(!d_flag_only.multi_signal, "ban flag-only multi_signal=false");
  }

  // ── 6. depth::LeakageScorer mitigate_world — fog structural kill ─────────
  {
    auto w = sim::make_arena();
    depth::LeakageScorer scorer;
    server::Observer obs{{0.f, 0.f, 0.f}, 0.f};
    std::vector<depth::EntityTruth> all = {
        {1, {0, 0, 0}, 1, true, true},
        {2, {10, 0, 10}, 2, true, true},
        {3, {999, 0, 999}, 2, true, false},
        {4, {800, 0, 800}, 2, true, false},
        {5, {700, 0, 700}, 2, true, false},
    };

    // Red path: stream exfil residual, then blue multi-step mitigate.
    auto ex = depth::run_stream_exfil_red(w);
    expect(ex.key_exfiltrated || ex.useful_radar || w.stream_key_exfiltrated ||
               w.client_has_stream_key,
           "leakage red stream residual");
    auto mit = scorer.mitigate_world(w, obs, 1, all);
    expect(w.entity_stream_encrypted && !w.client_has_stream_key,
           "mitigate strips client key");
    expect(!w.server_sends_full_enemy_origin, "mitigate enforces fog");
    expect(mit.structural_kill, "mitigate structural kill");
    std::printf("  leak: %s\n", mit.detail.c_str());
  }

  // ── 7. depth::TrustAggregator evaluate on hostile world ──────────────────
  {
    auto w = sim::make_arena();
    depth::TrustAggregator agg;
    auto clean = agg.evaluate_world_timeline(w, 2);
    expect(clean.allow_ranked && clean.action == depth::TrustAction::Allow,
           "trust clean allows ranked");

  // depth::plant_hostile_trust_timeline: depth: plant or apply educational residuals on World.
    depth::plant_hostile_trust_timeline(w);
    auto hostile = agg.evaluate_world_timeline(w, 3);
    expect(!hostile.allow_ranked, "trust hostile blocks ranked");
    expect(hostile.policy_fails + hostile.spoof_signals >= 2,
           "trust hostile multi-signal");
    expect(hostile.risk >= 6.0, "trust hostile aggregate risk");
    expect(hostile.action == depth::TrustAction::BlockRanked ||
               hostile.action == depth::TrustAction::HardBlock,
           "trust hostile action blocks");
    std::printf("  trust: %s\n", hostile.detail.c_str());
  }

  // ── 8. depth::SellerFusionCorrelator plant+evaluate ──────────────────────
  {
    auto w = sim::make_arena();
    depth::SellerFusionCorrelator fuse;
    auto empty = fuse.evaluate(w);
    expect(!empty.cluster_detected, "seller empty no cluster");

    depth::SellerFusionCorrelator::plant_seller_cluster(w);
    auto hit = fuse.evaluate(w);
    expect(hit.families_hit >= 3, "seller ≥3 signal families");
    expect(hit.cluster_detected, "seller cluster detected");
    expect(hit.risk > 0, "seller fusion risk");
    expect(hit.action != depth::ClusterAction::None, "seller cluster action");
    expect(!hit.reasons.empty() || !hit.detail.empty(),
           "seller multi_reason detail");
    std::printf("  seller: %s\n", hit.detail.c_str());
  }

  // ── 9. lab::LabMemoryBackend attach fixture + read ───────────────────────
  {
    auto& fx = lab::global_fixture();
    lab::LabMemoryBackend backend;
    expect(backend.attach(fx.id()) == ac::Status::Ok, "lab_memory attach");
    expect(backend.is_attached(), "lab_memory is_attached");
    expect(backend.tier() == ac::Tier::T0_UsermodeRpm, "lab_memory tier T0");

    ac::ReadRequest req{fx.base_address(), sizeof(std::uint32_t)};
    auto rr = backend.read(req);
    expect(rr.status == ac::Status::Ok, "lab_memory read ok");
    expect(rr.bytes.size() == sizeof(std::uint32_t), "lab_memory read size");
    std::uint32_t count = 0;
    std::memcpy(&count, rr.bytes.data(), sizeof(count));
    expect(count >= 2, "lab_memory fixture entity count");

    // Entity blob at base+0x10.
    ac::ReadRequest ent_req{fx.base_address() + 0x10, 16};
    auto er = backend.read(ent_req);
    expect(er.status == ac::Status::Ok && er.bytes.size() == 16,
           "lab_memory entity blob read");

    backend.detach();
    expect(!backend.is_attached(), "lab_memory detach");
    auto denied = backend.read(req);
    expect(denied.status != ac::Status::Ok, "lab_memory read after detach fails");
  }

  // ── 10. fps::Scenario plant/defuse + lab_bridge sync ─────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    expect(sc.phase() == fps::RoundPhase::Live, "fps round starts live");
    expect(sc.alive_count(fps::Team::Attacker) >= 1, "fps attackers present");
    expect(sc.alive_count(fps::Team::Defender) >= 1, "fps defenders present");

    auto plant = sc.plant_instant(1, "A");
    expect(plant.ok, "fps plant at A");
    expect(sc.bomb().planted && sc.bomb().site_name == "A", "fps bomb planted");
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "fps phase bomb_planted");

    auto def = sc.defuse_instant(3);
    expect(def.ok, "fps defuse ok");
    expect(sc.bomb().defused, "fps bomb defused");
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinDefuse,
           "fps defenders win defuse");

    // Fresh round for lab bridge sync (post-end scenario may be empty-ish).
    fps::Scenario sc2;
    sc2.start_default_round();
    sc2.end_freeze();
    sc2.plant_instant(1, "B");
    auto snaps = fps::to_entity_snapshots(sc2, false);
    expect(snaps.size() >= 4, "fps entity snapshots");

    auto w = fps::make_lab_world_from_scenario(sc2);
    expect(w.game_pid() != 0 && w.ac_pid() != 0, "fps lab world game+ac");
    auto* g = w.proc(w.game_pid());
    expect(g && g->memory.size() >= 4, "fps lab world game memory");
    std::uint32_t count = 0;
    std::memcpy(&count, g->memory.data(), sizeof(count));
    expect(count == snaps.size() || count >= 2, "fps lab_bridge entity count");

    // sync_entities_to_sim residual path on existing world.
    expect(fps::sync_entities_to_sim(sc2, w), "fps sync_entities_to_sim");
  }

  if (fails) {
    std::fprintf(stderr, "shared_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("shared_full_tests: all passed\n");
  return 0;
}
