// Tools surface full tests: strategy catalog runner + FPS plant path used by
// fps_demo / strategy_lab. Drives shipped APIs only (no stubs).

#include "fps/lab_bridge.hpp"
#include "fps/scenario.hpp"
#include "strategies/framework.hpp"

#include <cstdio>
#include <cstring>
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
  // ── 1–3. strategies catalog surface ────────────────────────────────────
  {
    const auto& cat = strategies::catalog();
    // Shipped strategy_lab catalog (pairs 01–53 live).
    expect(cat.size() >= 56u, "strategies::catalog().size() >= 56");

    const auto* rpm = strategies::find("01_external_rpm");
    expect(rpm != nullptr, "strategies::find(01_external_rpm) non-null");
    if (rpm) {
      expect(std::string(rpm->meta.id) == "01_external_rpm",
             "find id matches 01_external_rpm");
    }

    expect(strategies::find("nope") == nullptr,
           "strategies::find(nope) null");
  }

  // ── 4–5. run_one structural strategies (quiet) ─────────────────────────
  {
    expect(strategies::run_one("24_interest_mgmt", false) == 0,
           "run_one(24_interest_mgmt) == 0");
    expect(strategies::run_one("26_delayed_ban", false) == 0,
           "run_one(26_delayed_ban) == 0");
    expect(strategies::run_one("29_pattern_offset_scan", false) == 0,
           "run_one(29_pattern_offset_scan) == 0");
    expect(strategies::run_one("30_fp_allowlist_evasion", false) == 0,
           "run_one(30_fp_allowlist_evasion) == 0");
    expect(strategies::run_one("31_composition_radar_loop", false) == 0,
           "run_one(31_composition_radar_loop) == 0");
    expect(strategies::run_one("32_cs_round_radar", false) == 0,
           "run_one(32_cs_round_radar) == 0");
    expect(strategies::run_one("33_handle_hijack_proxy", false) == 0,
           "run_one(33_handle_hijack_proxy) == 0");
    expect(strategies::run_one("34_bomb_round_intel", false) == 0,
           "run_one(34_bomb_round_intel) == 0");
    expect(strategies::run_one("35_spectator_feed", false) == 0,
           "run_one(35_spectator_feed) == 0");
    expect(strategies::run_one("36_triggerbot_timing", false) == 0,
           "run_one(36_triggerbot_timing) == 0");
    expect(strategies::run_one("37_rcs_pattern", false) == 0,
           "run_one(37_rcs_pattern) == 0");
    expect(strategies::run_one("38_sound_esp", false) == 0,
           "run_one(38_sound_esp) == 0");
    expect(strategies::run_one("39_streamproof_overlay", false) == 0,
           "run_one(39_streamproof_overlay) == 0");
    expect(strategies::run_one("14_offset_c2", false) == 0,
           "run_one(14_offset_c2) == 0");
    expect(strategies::run_one("40_no_flash", false) == 0,
           "run_one(40_no_flash) == 0");
    expect(strategies::run_one("41_projectile_nade_esp", false) == 0,
           "run_one(41_projectile_nade_esp) == 0");
    expect(strategies::run_one("42_object_glow_product", false) == 0,
           "run_one(42_object_glow_product) == 0");
    expect(strategies::run_one("43_plant_defuse_hud", false) == 0,
           "run_one(43_plant_defuse_hud) == 0");
    expect(strategies::run_one("44_internal_footprint", false) == 0,
           "run_one(44_internal_footprint) == 0");
    expect(strategies::run_one("45_protector_suite", false) == 0,
           "run_one(45_protector_suite) == 0");
    expect(strategies::run_one("46_fov_viewmodel_mod", false) == 0,
           "run_one(46_fov_viewmodel_mod) == 0");
    expect(strategies::run_one("47_noscope_inaccuracy_viz", false) == 0,
           "run_one(47_noscope_inaccuracy_viz) == 0");
    expect(strategies::run_one("48_sound_viz_subtypes", false) == 0,
           "run_one(48_sound_viz_subtypes) == 0");
    expect(strategies::run_one("49_crosshair_helper", false) == 0,
           "run_one(49_crosshair_helper) == 0");
    expect(strategies::run_one("50_desktop_dup_capture", false) == 0,
           "run_one(50_desktop_dup_capture) == 0");
    expect(strategies::run_one("51_dll_thread_protect", false) == 0,
           "run_one(51_dll_thread_protect) == 0");
    expect(strategies::run_one("52_schema_saas_product", false) == 0,
           "run_one(52_schema_saas_product) == 0");
    expect(strategies::run_one("53_multimap_radar_share", false) == 0,
           "run_one(53_multimap_radar_share) == 0");
    expect(strategies::run_one("48_sound_viz_subtypes", false) == 0,
           "run_one(48_sound_viz_subtypes) == 0");
    expect(strategies::run_one("42_object_glow_product", false) == 0,
           "run_one(42_object_glow_product) == 0");
    expect(strategies::run_one("45_protector_suite", false) == 0,
           "run_one(45_protector_suite) == 0");
    expect(strategies::run_one("54_lag_switch", false) == 0,
           "run_one(54_lag_switch) == 0");
    expect(strategies::run_one("55_network_multibox_aim", false) == 0,
           "run_one(55_network_multibox_aim) == 0");
    expect(strategies::run_one("56_iommu_policy", false) == 0,
           "run_one(56_iommu_policy) == 0");
  }

  // ── 6. family_name / run_filtered (strategy_lab expansion) ─────────────
  {
    expect(std::string(strategies::family_name(strategies::Family::Structural)) ==
               "Structural",
           "family_name(Structural)");
    expect(strategies::run_filtered("Structural", nullptr, false, nullptr) == 0,
           "run_filtered(Structural) == 0");
  }

  // ── 7. FPS plant_instant path (shared with fps_demo) ───────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    // CS-inspired Scenario starts in Buy freeze; plant_instant ends freeze.
    expect(sc.phase() == fps::RoundPhase::Buy ||
               sc.phase() == fps::RoundPhase::Live,
           "fps round buy or live");
    expect(sc.alive_count(fps::Team::Attacker) >= 1, "fps attackers");
    expect(sc.alive_count(fps::Team::Defender) >= 1, "fps defenders");

    auto plant = sc.plant_instant(1, "A");
    expect(plant.ok, "fps plant_instant A");
    expect(sc.bomb().planted && sc.bomb().site_name == "A", "fps bomb at A");
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "fps phase planted");

    auto snaps = fps::to_entity_snapshots(sc, /*living_only=*/false);
    expect(snaps.size() >= 4, "fps entity snapshots >= 4");

    auto w = fps::make_lab_world_from_scenario(sc);
    expect(w.game_pid() != 0, "fps lab world game_pid");
    auto* g = w.proc(w.game_pid());
    expect(g && g->memory.size() >= 4, "fps lab world game memory");
    std::uint32_t count = 0;
    std::memcpy(&count, g->memory.data(), sizeof(count));
    expect(count >= 2, "fps lab_bridge entity count >= 2");
  }

  if (fails) {
    std::fprintf(stderr, "tools_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("tools_full_tests: all passed\n");
  return 0;
}
