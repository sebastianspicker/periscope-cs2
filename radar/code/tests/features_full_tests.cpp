// Features full tests: overlay/phone/aim/input theme scars.

#include "sim/world.hpp"
#include "strategies/crosscutting/aim_humanization/red_example.hpp"
#include "strategies/crosscutting/aim_humanization/blue_example.hpp"
#include "strategies/crosscutting/silent_aim_desync/red_example.hpp"
#include "strategies/crosscutting/silent_aim_desync/blue_example.hpp"
#include "strategies/crosscutting/input_synthesis/red_example.hpp"
#include "strategies/crosscutting/input_synthesis/blue_example.hpp"
#include "strategies/crosscutting/overlay_esp/red_example.hpp"
#include "strategies/crosscutting/overlay_esp/blue_example.hpp"
#include "strategies/crosscutting/web_phone_radar/red_example.hpp"
#include "strategies/crosscutting/web_phone_radar/blue_example.hpp"
#include "strategies/crosscutting/triggerbot_timing/red_example.hpp"
#include "strategies/crosscutting/triggerbot_timing/blue_example.hpp"
#include "strategies/crosscutting/rcs_pattern/red_example.hpp"
#include "strategies/crosscutting/rcs_pattern/blue_example.hpp"
#include "strategies/crosscutting/sound_esp/red_example.hpp"
#include "strategies/crosscutting/sound_esp/blue_example.hpp"
#include "strategies/crosscutting/streamproof_overlay/red_example.hpp"
#include "strategies/crosscutting/streamproof_overlay/blue_example.hpp"
#include "strategies/crosscutting/no_flash/red_example.hpp"
#include "strategies/crosscutting/no_flash/blue_example.hpp"
#include "strategies/crosscutting/projectile_nade_esp/red_example.hpp"
#include "strategies/crosscutting/projectile_nade_esp/blue_example.hpp"
#include "strategies/crosscutting/fov_viewmodel_mod/red_example.hpp"
#include "strategies/crosscutting/fov_viewmodel_mod/blue_example.hpp"
#include "strategies/crosscutting/noscope_inaccuracy_viz/red_example.hpp"
#include "strategies/crosscutting/noscope_inaccuracy_viz/blue_example.hpp"
#include "strategies/crosscutting/sound_viz_subtypes/red_example.hpp"
#include "strategies/crosscutting/sound_viz_subtypes/blue_example.hpp"
#include "strategies/crosscutting/crosshair_helper/red_example.hpp"
#include "strategies/crosscutting/crosshair_helper/blue_example.hpp"
#include "strategies/crosscutting/desktop_dup_capture/red_example.hpp"
#include "strategies/crosscutting/desktop_dup_capture/blue_example.hpp"
#include "strategies/crosscutting/multimap_radar_share/red_example.hpp"
#include "strategies/crosscutting/multimap_radar_share/blue_example.hpp"

#include <cstdio>

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
  {
    auto w = sim::make_arena();
    auto rr = examples::aim_humanization::apply(w);
    expect(rr.achieved && w.inputs.size() >= 8, "aim soft multi inputs");
    bool soft = true;
    for (const auto& e : w.inputs) {
      if (e.dx > 40.f || e.dy > 40.f) soft = false;
    }
    expect(soft, "aim soft not snap");
    bool saas = false;
    for (const auto& n : w.net) {
      if (n.looks_like_radar_saas) saas = true;
    }
    expect(saas, "aim radar saas residual");
    auto br = examples::aim_humanization::detect(w);
    expect(br.detected, "aim residual detect (not snap-only)");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::silent_aim_desync::apply(w);
    expect(rr.achieved && w.silent_aim_active, "silent aim flag");
    expect(w.aim_samples.size() >= 4, "silent multi samples");
    auto br = examples::silent_aim_desync::detect(w);
    expect(br.detected, "silent blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::input_synthesis::apply(w);
    expect(rr.achieved && w.inputs.size() >= 3, "input synth multi");
    auto br = examples::input_synthesis::detect(w);
    expect(br.detected, "input synth blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::overlay_esp::apply(w);
    expect(rr.achieved && !w.overlays.empty(), "overlay world scar");
    auto br = examples::overlay_esp::detect(w);
    expect(br.detected, "overlay+handle composition");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::web_phone_radar::apply(w);
    expect(rr.achieved, "phone red");
    bool saas = false;
    for (const auto& n : w.net) {
      if (n.looks_like_radar_saas) saas = true;
    }
    expect(saas, "phone saas net");
    auto br = examples::web_phone_radar::detect(w);
    expect(br.detected, "phone handle+saas");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::triggerbot_timing::apply(w);
    expect(rr.achieved && w.triggerbot_active, "trigger red active");
    expect(w.trigger_on_target_fires >= 6, "trigger multi fires");
    expect(w.trigger_mean_latency_ms < 20.f, "trigger superhuman mean");
    auto br = examples::triggerbot_timing::detect(w);
    expect(br.detected && br.mitigated, "trigger blue");
    expect(br.mean_latency_ms < 50.f, "trigger blue latency floor");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::rcs_pattern::apply(w);
    expect(rr.achieved && w.rcs_active, "rcs red active");
    expect(w.rcs_pattern_samples >= 8, "rcs multi samples");
    auto br = examples::rcs_pattern::detect(w);
    expect(br.detected && br.mitigated, "rcs blue fit");
    expect(br.fit_samples >= 6 && br.max_err < 0.05f, "rcs near-perfect fit");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::sound_esp::apply(w);
    expect(rr.achieved && w.sound_esp_active, "sound_esp red");
    expect(w.sound_inferred_without_los >= 4, "sound_esp multi");
    auto br = examples::sound_esp::detect(w);
    expect(br.detected && br.mitigated, "sound_esp blue");
    expect(br.sound_only >= 3, "sound_esp audio-only count");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::streamproof_overlay::apply(w);
    expect(rr.achieved, "streamproof red");
    expect(!w.overlays.empty() && w.overlays.back().stream_proof,
           "streamproof overlay scar");
    expect(!w.capture_sees_overlays, "streamproof capture miss");
    auto br = examples::streamproof_overlay::detect(w);
    expect(br.detected && br.capture_miss, "streamproof blue present/capture");
    expect(br.mitigated && w.capture_sees_overlays, "streamproof capture parity");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::no_flash::apply(w);
    expect(rr.achieved && w.no_flash_active, "no_flash red");
    expect(w.flash_alpha_forced < 0.1f, "no_flash alpha strip");
    auto br = examples::no_flash::detect(w);
    expect(br.detected && br.fx_strip, "no_flash blue");
    expect(w.flash_alpha_forced >= 1.f || !w.no_flash_active, "no_flash restored");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::projectile_nade_esp::apply(w);
    expect(rr.achieved && w.projectile_esp_active, "projectile red");
    expect(w.nade_prediction_active && rr.arcs >= 1, "nade prediction");
    auto br = examples::projectile_nade_esp::detect(w);
    expect(br.detected && br.mitigated, "projectile blue");
    expect(!w.projectile_esp_active, "projectile cleared");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::fov_viewmodel_mod::apply(w);
    expect(rr.achieved && w.fov_mod_active, "fov red");
    expect(w.client_fov_override > 90.f && w.viewmodel_fov_override > 0.f,
           "fov both knobs");
    auto br = examples::fov_viewmodel_mod::detect(w);
    expect(br.detected && br.mitigated, "fov blue multi-reason");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::noscope_inaccuracy_viz::apply(w);
    expect(rr.achieved && w.noscope_inaccuracy_viz, "noscope red");
    expect(w.noscope_spread_shown > 0.1f, "noscope spread");
    auto br = examples::noscope_inaccuracy_viz::detect(w);
    expect(br.detected && br.mitigated, "noscope blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::sound_viz_subtypes::apply(w);
    expect(rr.achieved && w.sound_viz_subtypes_active, "sound subtypes red");
    expect(!w.sound_esp_active, "sound subtypes not generic 38");
    expect(w.sound_panel_reload_hit && w.sound_panel_scope_hit &&
               w.sound_panel_bomb_beep_hit && w.sound_panel_footstep_hit,
           "sound independent panel hits planted");
    expect(w.sound_viz_reload + w.sound_viz_scope + w.sound_viz_bomb_beep >= 3,
           "sound multi subtype counts");
    auto br = examples::sound_viz_subtypes::detect(w);
    expect(br.detected && br.mitigated, "sound subtypes blue");
    expect(br.independent_hits >= 3, "sound blue independent multi-reason");
    expect(br.hit_reload && br.hit_scope && br.hit_bomb_beep,
           "sound blue scores subtypes independently");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::crosshair_helper::apply(w);
    expect(rr.achieved && w.crosshair_helper_active, "crosshair red");
    expect(!w.triggerbot_active, "crosshair not triggerbot");
    auto br = examples::crosshair_helper::detect(w);
    expect(br.detected && br.mitigated, "crosshair blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::desktop_dup_capture::apply(w);
    expect(rr.achieved && w.desktop_duplication, "desktop_dup red");
    expect(w.present_path_has_overlay && !w.capture_sees_overlays,
           "desktop_dup present vs capture");
    auto br = examples::desktop_dup_capture::detect(w);
    expect(br.detected && w.capture_sensor_active, "desktop_dup sensor");
    expect(w.capture_vs_present_mismatch, "desktop_dup mismatch");
    expect(br.mitigated && w.capture_sees_overlays, "desktop_dup parity");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::multimap_radar_share::apply(w);
    expect(rr.achieved && w.multimap_radar_share, "multimap red");
    expect(w.radar_maps_shared >= 2 && !w.radar_share_token.empty(),
           "multimap share product");
    auto br = examples::multimap_radar_share::detect(w);
    expect(br.detected && br.mitigated, "multimap blue");
  }

  if (fails) {
    std::fprintf(stderr, "features_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("features_full_tests: all passed\n");
  return 0;
}
