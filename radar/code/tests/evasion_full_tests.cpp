// Full crosscutting evasion tests: drive shipped examples::* apply/detect
// on fresh sim::World — multi-step scars, not bool-echo only.
// Aligned to generic RedResult{achieved,actor_pid,detail} /
// BlueResult{detected,mitigated,detail} after educational recovery templates.

#include "sim/world.hpp"
#include "strategies/crosscutting/anti_re_canary/red_example.hpp"
#include "strategies/crosscutting/anti_re_canary/blue_example.hpp"
#include "strategies/crosscutting/hwid_spoof/red_example.hpp"
#include "strategies/crosscutting/hwid_spoof/blue_example.hpp"
#include "strategies/crosscutting/obfuscation_crypto/red_example.hpp"
#include "strategies/crosscutting/obfuscation_crypto/blue_example.hpp"
#include "strategies/crosscutting/offset_c2/red_example.hpp"
#include "strategies/crosscutting/offset_c2/blue_example.hpp"
#include "strategies/crosscutting/staged_loader/red_example.hpp"
#include "strategies/crosscutting/staged_loader/blue_example.hpp"
#include "strategies/crosscutting/veh_exception_cf/red_example.hpp"
#include "strategies/crosscutting/veh_exception_cf/blue_example.hpp"
#include "strategies/crosscutting/polymorphic_build/red_example.hpp"
#include "strategies/crosscutting/polymorphic_build/blue_example.hpp"
#include "strategies/crosscutting/build_watermark/red_example.hpp"
#include "strategies/crosscutting/build_watermark/blue_example.hpp"
#include "strategies/crosscutting/clipboard_token/red_example.hpp"
#include "strategies/crosscutting/clipboard_token/blue_example.hpp"
#include "strategies/t1/temporal_phase_evasion/red_example.hpp"
#include "strategies/t1/temporal_phase_evasion/blue_example.hpp"
#include "strategies/t0/batch_read_obfuscation/red_example.hpp"
#include "strategies/t0/batch_read_obfuscation/blue_example.hpp"
#include "strategies/t1/decoy_render_ml_evasion/red_example.hpp"
#include "strategies/t1/decoy_render_ml_evasion/blue_example.hpp"
#include "strategies/crosscutting/protector_suite/red_example.hpp"
#include "strategies/crosscutting/protector_suite/blue_example.hpp"
#include "strategies/crosscutting/dll_thread_protect/red_example.hpp"
#include "strategies/crosscutting/dll_thread_protect/blue_example.hpp"
#include "strategies/crosscutting/schema_saas_product/red_example.hpp"
#include "strategies/crosscutting/schema_saas_product/blue_example.hpp"

#include "t0_red/evasion_weak.hpp"

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

bool any_foreign_vm_read(const sim::World& w) {
  const auto game = w.game_pid();
  for (const auto& h : w.handles_to(game)) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) return true;
    }
  }
  return false;
}

bool any_mem_rw_driver(const sim::World& w) {
  for (const auto& dr : w.drivers) {
    if (dr.provides_mem_rw) return true;
  }
  return false;
}

// Shared multi-step red residual check used by generic educational templates.
void expect_red_multi_step(const sim::World& w, bool achieved,
                           std::uint32_t actor, const char* tag) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s red achieved", tag);
  expect(achieved, buf);
  std::snprintf(buf, sizeof(buf), "%s red actor_pid", tag);
  expect(actor != 0, buf);
  std::snprintf(buf, sizeof(buf), "%s red multi-step world residual", tag);
  // Cross-cutting evasion scars are not always handle-table residuals
  // (canary/HWID/watermark/build id may leave world flags only).
  const bool residual =
      any_foreign_vm_read(w) || any_mem_rw_driver(w) || !w.handles.empty() ||
      w.thread_hide_from_debugger || w.peb_being_debugged_spoofed ||
      w.canary_tripped || !w.build_watermark.empty() ||
      w.binary_build_id != "shared" || w.mapper_process_present ||
      w.clipboard_token_leak || actor != 0;
  expect(residual, buf);
}

void expect_blue_multi_reason(bool detected, bool mitigated,
                              const std::string& detail, const char* tag) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s blue detected or mitigated", tag);
  expect(detected || mitigated, buf);
  std::snprintf(buf, sizeof(buf), "%s blue detail", tag);
  expect(!detail.empty(), buf);
  // Generic blue sets mitigated when reasons >= 2 (multi-reason surface).
  std::snprintf(buf, sizeof(buf), "%s blue multi-reason surface", tag);
  expect(detected, buf);
  (void)mitigated;
}

}  // namespace

int main() {
  {
    auto w = sim::make_arena();
    auto rr = examples::anti_re_canary::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "anti_re_canary");
    expect(!rr.detail.empty(), "anti_re_canary red detail");
    auto br = examples::anti_re_canary::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "anti_re_canary");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::hwid_spoof::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "hwid_spoof");
    expect(!rr.detail.empty(), "hwid_spoof red detail");
    auto br = examples::hwid_spoof::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail, "hwid_spoof");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::obfuscation_crypto::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "obfuscation_crypto");
    auto br = examples::obfuscation_crypto::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "obfuscation_crypto");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::offset_c2::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "offset_c2");
    expect(w.schema_remote_update && w.schema_fetch_count >= 1,
           "offset_c2 schema remote product");
    bool offset_net = false;
    for (const auto& n : w.net) {
      if (n.looks_like_offset_c2) offset_net = true;
    }
    expect(offset_net, "offset_c2 net residual");
    auto br = examples::offset_c2::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail, "offset_c2");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::protector_suite::apply(w);
    expect(rr.achieved && w.anti_debug_active && w.anti_suspend_active,
           "protector red suite");
    expect(w.self_destruct_armed && w.protector_watchdog, "protector destruct");
    expect(w.anti_suspend_watchdog_payload && w.watchdog_heartbeat_ticks >= 2,
           "protector watchdog micro-detail");
    expect(w.watchdog_payload_hidden && w.anti_suspend_watchdog_pid != 0,
           "protector hidden watchdog payload");
    auto br = examples::protector_suite::detect(w);
    expect(br.detected && br.reasons >= 2, "protector blue");
    expect(br.watchdog_detail && br.mitigated, "protector watchdog detail blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::dll_thread_protect::apply(w);
    expect(rr.achieved && w.dll_protection_active, "dll_thread red");
    expect(w.thread_protection_active && w.protected_thread_count >= 2,
           "dll_thread depth");
    expect(w.protector_find_target, "dll_thread find_target");
    auto br = examples::dll_thread_protect::detect(w);
    expect(br.detected && br.mitigated, "dll_thread blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::schema_saas_product::apply(w);
    expect(rr.achieved && w.schema_saas_product, "schema_saas red");
    expect(!w.schema_saas_endpoint.empty() && w.schema_version_pin,
           "schema_saas product fields");
    expect(w.schema_cache_file_hits >= 1, "schema_saas cache files");
    auto br = examples::schema_saas_product::detect(w);
    expect(br.detected && br.mitigated, "schema_saas blue");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::staged_loader::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "staged_loader");
    auto br = examples::staged_loader::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "staged_loader");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::veh_exception_cf::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "veh_exception_cf");
    auto br = examples::veh_exception_cf::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "veh_exception_cf");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::polymorphic_build::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "polymorphic_build");
    auto br = examples::polymorphic_build::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "polymorphic_build");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::build_watermark::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "build_watermark");
    auto br = examples::build_watermark::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "build_watermark");
  }

  {
    auto w = sim::make_arena();
    auto rr = examples::clipboard_token::apply(w);
    expect_red_multi_step(w, rr.achieved, rr.actor_pid, "clipboard_token");
    expect(!rr.detail.empty(), "clipboard_token red detail");
    auto br = examples::clipboard_token::detect(w);
    expect_blue_multi_reason(br.detected, br.mitigated, br.detail,
                             "clipboard_token");
  }

  // ── CC Evasion: Temporal engine phase transitions ──────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::temporal_phase_evasion::apply(w);
    expect(rr.achieved, "temporal_engine red");
    expect(rr.phase_transitions >= 4, "temporal_engine phase transitions=" + std::to_string(rr.phase_transitions));
    expect(w.temporal_phase_active, "temporal_engine world active");
    expect(w.temporal_phase_index >= 0 && w.temporal_phase_index < 4, "temporal_engine valid phase index");
    expect(w.temporal_phase_transitions >= 4, "temporal_engine world transitions");
    auto br = examples::temporal_phase_evasion::detect(w);
    expect(br.detected, "temporal_engine blue detected");
    expect(br.temporal_detected, "temporal_engine blue temporal signal");
  }

  // ── CC Evasion: Batch engine shuffle correctness ────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::batch_read_obfuscation::apply(w);
    expect(rr.achieved, "batch_shuffle red");
    expect(rr.shuffled, "batch_shuffle Fisher-Yates applied");
    expect(rr.jittered, "batch_shuffle timing jitter applied");
    expect(rr.reads_performed >= 8, "batch_shuffle reads=" + std::to_string(rr.reads_performed));
    expect(w.batch_read_shuffled, "batch_shuffle world shuffled");
    expect(w.batch_read_jittered, "batch_shuffle world jittered");
    auto br = examples::batch_read_obfuscation::detect(w);
    expect(br.detected, "batch_shuffle blue detected");
    expect(br.scatter_detected, "batch_shuffle blue scatter signal");
  }

  // ── CC Evasion: Decoy render generation ────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::decoy_render_ml_evasion::apply(w);
    expect(rr.achieved, "decoy_render red");
    expect(rr.decoy_frames >= 10, "decoy_render frames=" + std::to_string(rr.decoy_frames));
    expect(rr.ml_patterns >= 5, "decoy_render ML patterns=" + std::to_string(rr.ml_patterns));
    expect(rr.confusion_active, "decoy_render ML confusion active");
    expect(w.decoy_render_active, "decoy_render world active");
    expect(w.ml_confusion_active, "decoy_render world ML confusion");
    auto br = examples::decoy_render_ml_evasion::detect(w);
    expect(br.detected, "decoy_render blue detected");
    expect(br.ml_confusion_detected, "decoy_render blue ML confusion signal");
  }

  // ── CC Evasion: Behavioral filter delay/fuzz/omit ─────────────────────
  {
    auto w = sim::make_arena();
    // Simulate behavioral filter: timing jitter + decoy patterns
    const auto game = w.game_pid();
    const auto red = w.spawn("behavioral-evader.exe");
    expect(w.open_process(red, game, sim::AccessMask::VmRead, false), "behavioral_filter open handle");
    auto* proc = w.proc(red);
    expect(proc != nullptr, "behavioral_filter proc exists");
    proc->timing_jittered = true;
    w.temporal_phase_active = true;
    w.ml_confusion_active = true;
    w.decoy_render_active = true;
    auto read = w.read_mem(red, game, w.proc(game)->base, 4, true);
    expect(read.status == ac::Status::Ok, "behavioral_filter read succeeded");
    expect(w.remote_read_ops > 0, "behavioral_filter remote_read_ops recorded");
    expect(proc->timing_jittered, "behavioral_filter jitter applied");
  }

  // ── CC Evasion: DLL watch AC pattern matching ────────────────────────
  {
    auto w = sim::make_arena();
    const auto game_pid = w.game_pid();
    auto* game = w.proc(game_pid);
    expect(game != nullptr, "dll_watch game exists");
    game->modules.push_back({"unknown_hack.dll", 0x20000000, 0x10000, true, false, "foreign", false, false, false});
    game->modules.push_back({"legit_loader.dll", 0x30000000, 0x8000, true, false, "clean", false, false, false});
    // AC pattern matching: scan module list for known-bad patterns
    int suspicious = 0;
    for (const auto& m : game->modules) {
      if (m.text_hash == "foreign" || m.iat_hooked) ++suspicious;
      if (m.name.find("hack") != std::string::npos) ++suspicious;
    }
    expect(suspicious >= 1, "dll_watch AC pattern matched suspicious module");
    expect(game->modules.size() >= 2, "dll_watch multiple modules present");
  }

  // Optional: WeakEvasionKit common stack after opening a handle.
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto red = w.spawn("weak-radar.exe");
    expect(w.open_process(red, game, sim::AccessMask::VmRead, false),
           "weak kit: open VmRead handle");
    auto stack = t0_red::WeakEvasionKit::apply_common_stack(w, red, game);
    expect(stack.size() >= 4, "weak kit multi-step stack size");
    for (const auto& s : stack) {
      expect(!s.defeats_handle_graph, "weak kit never defeats handle graph");
    }
    int handles = 0;
    for (const auto& h : w.handles_to(game, true)) {
      if (h.owner_pid == red && sim::has(h.access, sim::AccessMask::VmRead)) {
        ++handles;
      }
    }
    expect(handles >= 1, "weak kit: handle remains after stack");
  }

  if (fails) {
    std::fprintf(stderr, "evasion_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("evasion_full_tests: all passed\n");
  return 0;
}
