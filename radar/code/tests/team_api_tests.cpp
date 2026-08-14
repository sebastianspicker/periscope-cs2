// Integration tests: call shipped red/blue APIs for every tier + strategy samples.
// Drives real entry points only — no reimplementation of detectors.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "ac/telemetry.hpp"
#include "strategies/framework.hpp"

#include "t0_red/cheat_client.hpp"
#include "t0_blue/ac_agent.hpp"
#include "t1_red/syscall_cheat.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t2_red/kernel_radar.hpp"
#include "t2_blue/kernel_ac.hpp"
#include "t3_red/hv_radar.hpp"
#include "t3_blue/platform_ac.hpp"
#include "t4_red/dma_radar.hpp"
#include "t4_blue/dma_defense.hpp"

// Strategy headers (code root is on the include path)
#include "strategies/t0/external_rpm/red_example.hpp"
#include "strategies/t0/external_rpm/blue_example.hpp"
#include "strategies/t1/indirect_syscall/red_example.hpp"
#include "strategies/t1/indirect_syscall/blue_example.hpp"
#include "strategies/t2/byovd/red_example.hpp"
#include "strategies/t2/byovd/blue_example.hpp"
#include "strategies/t3/hypervisor/red_example.hpp"
#include "strategies/t3/hypervisor/blue_example.hpp"
#include "strategies/t4/dma_hardware/red_example.hpp"
#include "strategies/t4/dma_hardware/blue_example.hpp"
#include "strategies/crosscutting/staged_loader/red_example.hpp"
#include "strategies/crosscutting/staged_loader/blue_example.hpp"
#include "strategies/crosscutting/interest_mgmt/red_example.hpp"
#include "strategies/crosscutting/interest_mgmt/blue_example.hpp"
#include "strategies/t0/process_cooccurrence/red_example.hpp"
#include "strategies/t0/process_cooccurrence/blue_example.hpp"
#include "strategies/t1/parent_lineage/red_example.hpp"
#include "strategies/t1/parent_lineage/blue_example.hpp"
#include "strategies/t2/callback_shadow/red_example.hpp"
#include "strategies/t2/callback_shadow/blue_example.hpp"
#include "strategies/t3/attestation/red_example.hpp"
#include "strategies/t3/attestation/blue_example.hpp"
#include "strategies/t4/capture_cv_hid/red_example.hpp"
#include "strategies/t4/capture_cv_hid/blue_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/red_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/blue_example.hpp"
#include "strategies/t0/handle_hide_on_enum/red_example.hpp"
#include "strategies/t0/handle_hide_on_enum/blue_example.hpp"
#include "strategies/t1/stack_spoof/red_example.hpp"
#include "strategies/t1/stack_spoof/blue_example.hpp"
#include "strategies/t2/early_load_race/red_example.hpp"
#include "strategies/t2/early_load_race/blue_example.hpp"
#include "strategies/t3/ept_hide_ac/red_example.hpp"
#include "strategies/t3/ept_hide_ac/blue_example.hpp"
#include "strategies/t4/dual_boot_posture/red_example.hpp"
#include "strategies/t4/dual_boot_posture/blue_example.hpp"
#include "strategies/t0/dynamic_api_table/red_example.hpp"
#include "strategies/t0/dynamic_api_table/blue_example.hpp"
#include "strategies/t1/temporal_phase_evasion/red_example.hpp"
#include "strategies/t1/temporal_phase_evasion/blue_example.hpp"
#include "strategies/t0/handle_hijack_proxy_donor/red_example.hpp"
#include "strategies/t0/handle_hijack_proxy_donor/blue_example.hpp"
#include "strategies/t0/hud_radar_parsing/red_example.hpp"
#include "strategies/t0/hud_radar_parsing/blue_example.hpp"
#include "strategies/t0/cvar_walk_resolve/red_example.hpp"
#include "strategies/t0/cvar_walk_resolve/blue_example.hpp"
#include "strategies/t2/callback_shadow_kernel/red_example.hpp"
#include "strategies/t2/callback_shadow_kernel/blue_example.hpp"
#include "strategies/t3/ept_memory_hiding/red_example.hpp"
#include "strategies/t3/ept_memory_hiding/blue_example.hpp"
#include "strategies/t4/dma_page_table_walk/red_example.hpp"
#include "strategies/t4/dma_page_table_walk/blue_example.hpp"

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
    t0_red::CheatClient red(w, "api-radar.exe");
    expect(red.attach_to_game(), "t0 red attach");
    expect(red.pull_entities() && !red.entities().empty(), "t0 red entities");
    ac::MemoryTelemetrySink sink;
    t0_blue::AcAgent blue(w, sink);
    expect(blue.scan_handles().handle_hit, "t0 blue handles");
  }
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat red(w);
    expect(red.attach_via_syscall() && red.pull_entities(), "t1 red");
    const auto game = w.game_pid();
    t1_blue::UsermodeHookTrap hooks;
    t1_blue::HandleTruthMonitor truth;
    expect(hooks.count_visible_opens(w, game) == 0, "t1 hooks blind");
    expect(truth.count_vm_read_handles(w, game) >= 1, "t1 handle truth");
  }
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar red(w);
    expect(red.bring_up(t2_red::KernelPath::Byovd), "t2 red bring_up");
    expect(red.pull_entities() && !red.has_game_handle(), "t2 red no handle");
    ac::MemoryTelemetrySink sink;
    t2_blue::KernelAc blue(w, sink);
    blue.blocklist_add("lab_byovd_hash_001");
    t2_red::CallbackState base{4, 3, true};
    auto det = blue.scan(base, base);
    expect(det.byovd || det.suspicious_device || det.unknown_memrw_driver,
           "t2 blue scar");
  }
  {
    auto w = sim::make_arena();
    t3_red::HvRadar red(w);
    red.force_disable_vbs_for_lab();
    expect(red.try_hv("ACLABHV") && red.open_bridge(), "t3 red hv+bridge");
    expect(red.attach_best().ok && red.pull_entities(), "t3 red entities");
    ac::MemoryTelemetrySink sink;
    t3_blue::PlatformAc blue(w, sink);
    auto det = blue.full();
    expect(!det.ranked_allowed || det.hv_anomaly || det.bridge_hit, "t3 blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = t4_red::apply(w);
    expect(rr.achieved && rr.achieved, "t4 red apply clean process");
    auto br = t4_blue::detect(w);
    expect(br.detected || br.mitigated, "t4 blue detect/mitigate");
  }

  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::external_rpm::apply(w).achieved, "t0 strat red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::external_rpm::detect(w).detected, "t0 strat blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::indirect_syscall::apply(w).achieved, "t1 strat red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::indirect_syscall::detect(w).detected, "t1 strat blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    // RED multi-step entry for `src` (narrated): plant scars, report red result.
    expect(examples::byovd::run_red(w, n).achieved, "t2 strat red");
    // BLUE multi-reason entry for `src` (narrated): sensors + optional mitigate.
    expect(examples::byovd::run_blue(w, n).detected, "t2 strat blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    // RED multi-step entry for `src` (narrated): plant scars, report red result.
    expect(examples::hypervisor::run_red(w, n).achieved, "t3 strat red");
    auto b = examples::hypervisor::run_blue(w, n);
    expect(b.detected || b.mitigated, "t3 strat blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    // RED multi-step entry for `src` (narrated): plant scars, report red result.
    expect(examples::dma_hardware::apply(w).achieved, "t4 strat red");
    auto b = examples::dma_hardware::detect(w);
    expect(b.detected || b.mitigated, "t4 strat blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::staged_loader::apply(w).achieved, "xc staged red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::staged_loader::detect(w).detected, "xc staged blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::interest_mgmt::apply(w).achieved, "xc interest red");
    auto b = examples::interest_mgmt::detect(w);
    expect(b.mitigated || b.detected, "xc interest blue");
  }

  // ── New sophisticated strategies (gap inventory 29–41 samples) ──
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::process_cooccurrence::apply(w).achieved, "t0 cooc red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::process_cooccurrence::detect(w).detected, "t0 cooc blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::parent_lineage::apply(w).achieved, "t1 lineage red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::parent_lineage::detect(w).detected, "t1 lineage blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto r = examples::callback_shadow::run_red(w, n);
    expect(r.achieved, "t2 shadow red");
    // BLUE multi-reason entry for `src` (narrated): sensors + optional mitigate.
    expect(examples::callback_shadow::run_blue(w, n).detected,
           "t2 shadow blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto r = examples::attestation::run_red(w, n);
    expect(r.achieved, "t3 attest red");
    // BLUE multi-reason entry for `src` (narrated): sensors + optional mitigate.
    expect(examples::attestation::run_blue(w, n).detected,
           "t3 attest blue deny");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::capture_cv_hid::apply(w).achieved, "t4 cv red");
    auto b = examples::capture_cv_hid::detect(w);
    expect(b.detected || b.mitigated, "t4 cv blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::ac_self_integrity::apply(w).achieved,
           "xc ac integrity red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::ac_self_integrity::detect(w).detected,
           "xc ac integrity blue");
  }
  // Wave 2 (42–52 samples)
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::handle_hide_on_enum::apply(w).achieved, "t0 hide red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::handle_hide_on_enum::detect(w).detected, "t0 hide blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::stack_spoof::apply(w).achieved, "t1 stack red");
    // BLUE multi-reason entry for `src`: detect and/or mitigate on World sensors.
    expect(examples::stack_spoof::detect(w).detected, "t1 stack blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    // RED multi-step entry for `src` (narrated): plant scars, report red result.
    expect(examples::early_load_race::run_red(w, n).achieved, "t2 early red");
    // BLUE multi-reason entry for `src` (narrated): sensors + optional mitigate.
    expect(examples::early_load_race::run_blue(w, n).detected,
           "t2 early blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    // RED multi-step entry for `src` (narrated): plant scars, report red result.
    expect(examples::ept_hide_ac::run_red(w, n).achieved, "t3 ept red");
    auto b = examples::ept_hide_ac::run_blue(w, n);
    expect(b.detected || b.mitigated, "t3 ept blue");
  }
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    expect(examples::dual_boot_posture::apply(w).achieved, "t4 dual red");
    auto b = examples::dual_boot_posture::detect(w);
    expect(b.detected || b.mitigated, "t4 dual blue");
  }

  // ── Enhanced team API: dynamic API resolution ────────────────────────
  {
    auto w = sim::make_arena();
    expect(examples::dynamic_api_table::apply(w).achieved, "team_api dynamic_api red");
    auto br = examples::dynamic_api_table::detect(w);
    expect(br.detected, "team_api dynamic_api blue");
    expect(br.dynamic_imports_detected, "team_api dynamic import signal");
  }
  // ── Enhanced team API: temporal phase engine ─────────────────────────
  {
    auto w = sim::make_arena();
    expect(examples::temporal_phase_evasion::apply(w).achieved, "team_api temporal red");
    auto br = examples::temporal_phase_evasion::detect(w);
    expect(br.detected, "team_api temporal blue");
    expect(br.temporal_detected, "team_api temporal signal");
  }
  // ── Enhanced team API: callback shadow kernel ────────────────────────
  {
    auto w = sim::make_arena();
    expect(examples::callback_shadow_kernel::apply(w).achieved, "team_api callback_shadow red");
    auto br = examples::callback_shadow_kernel::detect(w);
    expect(br.detected, "team_api callback_shadow blue");
    expect(br.callback_anomaly, "team_api callback anomaly signal");
  }
  // ── Enhanced team API: EPT memory hiding ─────────────────────────────
  {
    auto w = sim::make_arena();
    expect(examples::ept_memory_hiding::apply(w).achieved, "team_api ept_hiding red");
    auto br = examples::ept_memory_hiding::detect(w);
    expect(br.detected, "team_api ept_hiding blue");
    expect(br.ept_hiding_detected, "team_api ept hiding signal");
  }
  // ── Enhanced team API: DMA page table walk ───────────────────────────
  {
    auto w = sim::make_arena();
    expect(examples::dma_page_table_walk::apply(w).achieved, "team_api dma_walk red");
    auto br = examples::dma_page_table_walk::detect(w);
    expect(br.detected, "team_api dma_walk blue");
    expect(br.dma_walk_detected, "team_api dma walk signal");
  }

  {
    int cat_fail = 0;
    // strategies::catalog: Static list of StrategyEntry factories.
    for (const auto& e : strategies::catalog()) {
      auto w = sim::make_arena();
      sim::Narrator n;
      auto res = e.run(w, n);
      if (!(res.blue_detected || res.blue_mitigated || !res.red_achieved)) {
        std::fprintf(stderr, "FAIL catalog %s: %s\n", e.meta.id,
                     res.summary.c_str());
        ++cat_fail;
      }
    }
    expect(cat_fail == 0, "catalog all blue outcomes");
    expect(strategies::catalog().size() >= 56u, "catalog >= 56 (including 24 new)");
  }

  if (fails) {
    std::fprintf(stderr, "%d team_api_tests failure(s)\n", fails);
    return 1;
  }
  std::puts("team_api_tests passed");
  return 0;
}
