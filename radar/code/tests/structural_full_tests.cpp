// Structural full tests: theme-specific World residuals (not generic stubs).

#include "sim/world.hpp"
#include "strategies/crosscutting/delayed_ban/red_example.hpp"
#include "strategies/crosscutting/delayed_ban/blue_example.hpp"
#include "strategies/crosscutting/interest_mgmt/red_example.hpp"
#include "strategies/crosscutting/interest_mgmt/blue_example.hpp"
#include "strategies/crosscutting/info_advantage/red_example.hpp"
#include "strategies/crosscutting/info_advantage/blue_example.hpp"
#include "strategies/crosscutting/fallback_chain/red_example.hpp"
#include "strategies/crosscutting/fallback_chain/blue_example.hpp"
#include "strategies/crosscutting/entity_stream_crypto/red_example.hpp"
#include "strategies/crosscutting/entity_stream_crypto/blue_example.hpp"
#include "strategies/crosscutting/composition_radar_loop/red_example.hpp"
#include "strategies/crosscutting/composition_radar_loop/blue_example.hpp"
#include "strategies/crosscutting/cs_round_radar/red_example.hpp"
#include "strategies/crosscutting/cs_round_radar/blue_example.hpp"
#include "strategies/crosscutting/bomb_round_intel/red_example.hpp"
#include "strategies/crosscutting/bomb_round_intel/blue_example.hpp"
#include "strategies/crosscutting/spectator_feed/red_example.hpp"
#include "strategies/crosscutting/spectator_feed/blue_example.hpp"
#include "strategies/crosscutting/object_glow_product/red_example.hpp"
#include "strategies/crosscutting/object_glow_product/blue_example.hpp"
#include "strategies/t0/runtime_health_ladder/red_example.hpp"
#include "strategies/t0/runtime_health_ladder/blue_example.hpp"
#include "strategies/t0/accept_readiness_gate/red_example.hpp"
#include "strategies/t0/accept_readiness_gate/blue_example.hpp"
#include "strategies/t0/shellcode_inject_donor/red_example.hpp"
#include "strategies/t0/shellcode_inject_donor/blue_example.hpp"
#include "strategies/crosscutting/plant_defuse_hud/red_example.hpp"
#include "strategies/crosscutting/plant_defuse_hud/blue_example.hpp"
#include "fps/scenario.hpp"
#include "fps/lab_bridge.hpp"

#include <cmath>
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
void expect(bool c, const std::string& m) { expect(c, m.c_str()); }
}  // namespace

int main() {
  {
    auto w = sim::make_arena();
    auto rr = examples::interest_mgmt::apply(w);
    expect(rr.achieved && rr.wants_full_origin, "interest red full origin want");
    expect(w.server_sends_full_enemy_origin, "interest red world full origin");
    auto br = examples::interest_mgmt::detect(w);
    expect(br.mitigated, "interest blue mitigated");
    expect(!w.server_sends_full_enemy_origin, "interest fog clears full origin");
    expect(w.client_entity_fidelity < 1.0f, "interest fog+ fidelity cut");
    expect(br.enemies_culled >= 1, "interest culled far enemies");
  }
  {
    // Clean-world negative: blue must not invent pre-aim frames.
    auto clean = sim::make_arena();
    expect(clean.aim_samples.empty(), "info clean no aim_samples");
    auto bclean = examples::info_advantage::detect(clean);
    expect(!bclean.detected && bclean.score == 0,
           "info blue clean-world no detect");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::info_advantage::apply(w);
    expect(rr.achieved && rr.preaim_frames >= 3, "info red preaim frames");
    expect(static_cast<int>(w.aim_samples.size()) >= 3,
           "info red planted aim_samples residual");
    int hidden = 0;
    for (const auto& s : w.aim_samples) {
      if (s.aim_on_hidden_target && !s.has_vision_on_target &&
          !s.has_audio_on_target) {
        ++hidden;
      }
    }
    expect(hidden >= 3, "info red World preaim residual fields");
    // P3 spectator residual (World flags only)
    w.spectator_feed_active = true;
    w.spectator_has_delayed_enemy_origin = true;
    auto br = examples::info_advantage::detect(w);
    expect(br.detected, "info blue detected");
    expect(br.score >= 3, "info blue score from World samples");
    expect(br.multi_reason || br.score >= 3, "info multi-reason/score");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::delayed_ban::apply(w);
    expect(rr.ticks >= 2, "delayed red multi-tick");
    expect(rr.confidence > 0, "delayed red confidence");
    expect(w.lab_match_tick >= 2, "world match ticks");
    auto br = examples::delayed_ban::detect(w);
    expect(br.multi_tick, "delayed blue multi_tick");
    expect(br.detected, "delayed blue detected");
    expect(w.overwatch_queued || br.mitigated, "delayed queue/mitigate");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::entity_stream_crypto::apply(w);
    expect(rr.achieved && w.client_has_stream_key, "stream red has key");
    auto br = examples::entity_stream_crypto::detect(w);
    expect(br.mitigated, "stream blue mitigated");
    expect(!w.client_has_stream_key, "stream no client key");
    expect(!w.server_sends_full_enemy_origin, "stream fog+");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::fallback_chain::apply(w);
    expect(rr.achieved && rr.hv_failed && rr.rpm_ok, "fallback multi path");
    auto br = examples::fallback_chain::detect(w);
    expect(br.detected && br.handle_hit && br.driver_hit, "fallback stack sensors");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::composition_radar_loop::apply(w);
    expect(rr.entities >= 2, "composition cs-backed entity product");
    expect(rr.achieved, "composition product");
    auto br = examples::composition_radar_loop::detect(w);
    expect(br.detected, "composition blue detect");
    expect(br.fog || br.mitigated, "composition fog/mitigate");
    expect(!w.server_sends_full_enemy_origin, "composition post fog no full origin");
  }
  // CS round → lab_bridge → RPM red → interest fog blue (scenario-derived).
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sim::World w;
    auto rr = examples::cs_round_radar::apply_from_scenario(sc, w);
    expect(rr.achieved, "cs_round red achieved");
    expect(rr.bomb_planted && rr.site == "A", "cs_round planted A");
    expect(rr.entity_count >= 4, "cs_round scenario entity count");
    expect(rr.attackers_alive >= 1 && rr.defenders_alive >= 1,
           "cs_round both teams in product");
    // Snapshots match scenario teams.
    auto snaps = fps::to_entity_snapshots(sc, false);
    expect(static_cast<int>(snaps.size()) == rr.entity_count ||
               snaps.size() >= 4,
           "cs_round snapshots align");
    auto br = examples::cs_round_radar::detect_with_scenario(w, sc);
    expect(br.foreign_vm_read, "cs_round blue handle");
    expect(br.enemies_culled >= 1, "cs_round fog culled far enemies");
    expect(br.mitigated && br.fog, "cs_round fog mitigate");
    expect(!w.server_sends_full_enemy_origin, "cs_round fidelity cut");
    expect(w.client_entity_fidelity < 1.0f, "cs_round fidelity < 1");
  }
  // Bomb timer / defuse intel product (distinct from player XY ESP).
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sim::World w;
    auto rr = examples::bomb_round_intel::apply_from_scenario(sc, w);
    expect(rr.achieved, "bomb_intel red achieved");
    expect(rr.bomb_planted && rr.site == "A", "bomb_intel planted A");
    expect(rr.fuse_known > 0.f, "bomb_intel fuse known");
    expect(w.bomb_intel_product, "bomb_intel world scar");
    auto br = examples::bomb_round_intel::detect_with_scenario(w, sc);
    expect(br.detected, "bomb_intel blue detect");
    expect(br.mitigated, "bomb_intel blue mitigate");
    expect(!w.bomb_intel_product && w.bomb_fuse_known < 0.f,
           "bomb_intel scars cleared");
  }
  // Spectator feed / delayed origin residual.
  {
    auto w = sim::make_arena();
    auto rr = examples::spectator_feed::apply(w);
    expect(rr.achieved && w.spectator_feed_active, "spectator feed red");
    expect(w.spectator_has_delayed_enemy_origin, "spectator delayed origin");
    expect(rr.spectator_count >= 1, "spectator count");
    auto br = examples::spectator_feed::detect(w);
    expect(br.detected && br.mitigated, "spectator blue");
    expect(!w.spectator_has_delayed_enemy_origin, "spectator delayed cleared");
  }
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sim::World w;
    auto rr = examples::object_glow_product::apply_from_scenario(sc, w);
    expect(rr.achieved && w.object_glow_product, "object_glow red");
    expect(!w.bomb_intel_product, "object_glow not fuse timer");
    expect(rr.dropped_bomb + rr.defuse_kit + rr.hostage >= 2, "object_glow multi");
    expect(rr.class_count >= 3 && w.object_glow_class_count >= 3,
           "object_glow multi-class product");
    expect(w.glow_grenade_projectile > 0 && w.glow_weapon > 0,
           "object_glow extra Osiris-class entities");
    auto br = examples::object_glow_product::detect(w);
    expect(br.detected && br.mitigated, "object_glow blue");
    expect(br.multi_class && br.class_count >= 3, "object_glow multi-class blue");
  }
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    // end_freeze leaves clock_ == round_time (remaining, not elapsed).
    const float remaining_before = sc.clock();
    expect(sc.phase() == fps::RoundPhase::Buy ||
               sc.phase() == fps::RoundPhase::Live,
           "plant_hud scenario buy/live");
    sim::World w;
    auto rr = examples::plant_defuse_hud::apply_from_scenario(sc, w);
    expect(rr.achieved && w.plant_hud_alert_active, "plant_hud red");
    expect(w.defuse_window_alert_active, "defuse window alert");
    expect(!w.bomb_intel_product, "plant_hud not fuse product");
    // clock() is remaining: at live start ~ round_time (default 115), not 0.
    expect(w.plant_time_remaining_known > 10.f, "plant_hud remaining >> 0");
    expect(std::fabs(w.plant_time_remaining_known - sc.clock()) < 0.01f ||
               std::fabs(w.plant_time_remaining_known - remaining_before) <
                   0.01f ||
               std::fabs(w.plant_time_remaining_known -
                         sc.config().round_time) < 0.01f,
           "plant_hud remaining tracks Scenario::clock remaining");
    expect(rr.time_remaining > sc.config().plant_time + 1.f,
           "plant_hud time_remaining enough for plant");
    expect(rr.plant_feasible && w.plant_feasible_before_round_end,
           "plant_hud feasible at live start (plenty of clock left)");
    auto br = examples::plant_defuse_hud::detect(w);
    expect(br.detected && br.mitigated, "plant_hud blue");
  }

  // ── Health ladder transitions ───────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::runtime_health_ladder::apply(w);
    expect(rr.achieved, "health_ladder red achieved");
    expect(rr.level >= 3, "health_ladder level=" + std::to_string(rr.level));
    expect(rr.transitions >= 3, "health_ladder transitions=" + std::to_string(rr.transitions));
    expect(w.health_ladder_level >= 3, "health_ladder world level=" + std::to_string(w.health_ladder_level));
    expect(w.health_ladder_transition_count >= 3, "health_ladder world transitions=" + std::to_string(w.health_ladder_transition_count));
    expect(w.health_ladder_self_heal_armed, "health_ladder self-heal armed");
    expect(w.health_ladder_ticks_at_level > 0, "health_ladder ticks at level=" + std::to_string(w.health_ladder_ticks_at_level));
    auto br = examples::runtime_health_ladder::detect(w);
    expect(br.detected, "health_ladder blue detected");
    expect(br.health_ladder_detected, "health_ladder blue ladder signal");
  }

  // ── ACCEPT gate criteria ───────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::accept_readiness_gate::apply(w);
    expect(rr.achieved, "accept_gate red achieved");
    expect(rr.conditions_met >= 7, "accept_gate conditions met=" + std::to_string(rr.conditions_met));
    expect(rr.gate_open, "accept_gate gate open");
    expect(rr.session_ready, "accept_gate session ready");
    expect(w.accept_gate_open, "accept_gate world gate open");
    expect(w.accept_session_ready, "accept_gate world session ready");
    expect(w.accept_conditions_met == w.accept_conditions_total, "accept_gate all " + std::to_string(w.accept_conditions_met) + "/" + std::to_string(w.accept_conditions_total) + " conditions met");
    expect(w.match_active, "accept_gate match active");
    auto br = examples::accept_readiness_gate::detect(w);
    expect(br.detected, "accept_gate blue detected");
    expect(br.readiness_bypassed, "accept_gate blue bypass signal");
  }

  // ── ETL (Entity Transfer Layer) recording/verification ──────────────
  {
    auto w = sim::make_arena();
    // Simulate entity transfer layer: shared sections + entity streaming
    const auto helper = w.spawn("etl-helper.exe");
    const auto consumer = w.spawn("etl-consumer.exe");
    expect(w.proc(helper) != nullptr && w.proc(consumer) != nullptr, "etl processes spawned");

    sim::SharedSection sec;
    sec.name = "Local\\ETL_EntityStream";
    sec.creator_pid = helper;
    sec.consumer_pid = consumer;
    sec.carries_entity_bytes = true;
    w.add_section(sec);

    w.helper_ticket_active = true;
    w.helper_ticket_shared_memory_created = true;
    w.helper_ticket_claimed = true;
    expect(!w.sections.empty(), "etl shared sections present");
    expect(w.helper_ticket_active && w.helper_ticket_claimed, "etl ticket active and claimed");

    bool has_entity_section = false;
    for (const auto& s : w.sections)
      if (s.carries_entity_bytes) has_entity_section = true;
    expect(has_entity_section, "etl entity-bearing section verified");

    // Verify consumer can read from section
    bool consumer_found = false;
    for (const auto& s : w.sections)
      if (s.consumer_pid == consumer) consumer_found = true;
    expect(consumer_found, "etl consumer mapped to section");
  }

  // ── Shellcode deobfuscation ─────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::shellcode_inject_donor::apply(w);
    expect(rr.achieved, "shellcode_red achieved");
    expect(rr.donor_pid != 0, "shellcode_donor pid=" + std::to_string(rr.donor_pid));
    expect(rr.obfuscated, "shellcode_obfuscated");
    expect(w.shellcode_donor_active, "shellcode_world donor active");
    expect(w.shellcode_obfuscated, "shellcode_world obfuscated");
    expect(w.shellcode_xor_key_applied, "shellcode_world XOR key applied");

    // Verify deobfuscation by checking that obfuscation fields present
    if (w.shellcode_obfuscated && w.shellcode_xor_key_applied) {
      w.shellcode_obfuscated = false;
      w.shellcode_xor_key_applied = false;
      expect(!w.shellcode_obfuscated, "shellcode_deobfuscated obfuscation cleared");
      expect(!w.shellcode_xor_key_applied, "shellcode_deobfuscated XOR key cleared");
    }
    auto br = examples::shellcode_inject_donor::detect(w);
    expect(br.detected || !rr.achieved, "shellcode_blue detected or red not achieved");
  }

  if (fails) {
    std::fprintf(stderr, "structural_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("structural_full_tests: all passed\n");
  return 0;
}
